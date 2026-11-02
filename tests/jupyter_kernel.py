# Test for the native (xeus-based) Cadabra Jupyter kernel: drive it with
# jupyter_client, as Jupyter would. Usage:
#     python3 jupyter_kernel.py <path to cadabra-jupyter-kernel> <PYTHONPATH>
import os, sys, json, tempfile, time, queue, faulthandler
# Fail (with a traceback) rather than hang if the kernel stops answering.
faulthandler.dump_traceback_later(120, exit=True)
from jupyter_client.manager import KernelManager
from jupyter_client.kernelspec import KernelSpecManager

kernel_bin, pythonpath = sys.argv[1], sys.argv[2]
d = tempfile.mkdtemp()
os.makedirs(os.path.join(d, "kernels", "cadabra"))
json.dump({"display_name": "Cadabra2 (native)", "language": "python",
           "argv": [kernel_bin, "-f", "{connection_file}"],
           "env": {"PYTHONPATH": pythonpath, "ASAN_OPTIONS": "detect_leaks=0"}},
          open(os.path.join(d, "kernels", "cadabra", "kernel.json"), "w"))
ksm = KernelSpecManager(kernel_dirs=[os.path.join(d, "kernels")])
km = KernelManager(kernel_name="cadabra", kernel_spec_manager=ksm)
km.start_kernel()
kc = km.client(); kc.start_channels(); kc.wait_for_ready(timeout=60)

failures = []
def check(label, cond, detail=""):
    print(("PASS " if cond else "FAIL ") + label + ("" if cond else "   " + str(detail)[:300]))
    if not cond: failures.append(label)

def run(code, timeout=60):
    msg_id = kc.execute(code)
    reply = kc.get_shell_msg(timeout=timeout)
    while reply["parent_header"].get("msg_id") != msg_id:
        reply = kc.get_shell_msg(timeout=timeout)
    outputs = []
    while True:
        m = kc.get_iopub_msg(timeout=timeout)
        if m["parent_header"].get("msg_id") != msg_id: continue
        if m["msg_type"] == "status" and m["content"]["execution_state"] == "idle": break
        if m["msg_type"] in ("stream", "display_data", "execute_result", "error"):
            outputs.append((m["msg_type"], m["content"]))
    return reply["content"], outputs

kc.kernel_info(); info = kc.get_shell_msg(timeout=30)["content"]
check("kernel_info", info.get("implementation") == "cadabra_xeus_kernel", info)

r, out = run(r"ex:=A_{m} B_{m};")
latex = [c["data"].get("text/latex","") for t, c in out if t == "display_data"]
check("cadabra expression displays as LaTeX", r["status"] == "ok" and latex and "A_{m}" in latex[0] and "discretionary" not in latex[0], (r, out))

r, out = run("print(6*7)")
check("print goes to stdout stream", r["status"] == "ok" and ("stream", {"name": "stdout", "text": "42\n"}) in out, (r, out))

r, out = run("undefined_name_xyz")
errs = [c for t, c in out if t == "error"]
check("error reply", r["status"] == "error" and r.get("ename") == "NameError", r)
check("error published on iopub", errs and errs[0]["ename"] == "NameError", out)

r, out = run("print('still alive')")
check("kernel usable after error", r["status"] == "ok" and any(c.get("text") == "still alive\n" for t, c in out), (r, out))

for code, want in [("ex:=A+B;", "complete"), ("def f():", "incomplete"), ("x=1", "complete")]:
    kc.is_complete(code); rep = kc.get_shell_msg(timeout=30)["content"]
    check(f"is_complete {code!r} -> {want}", rep["status"] == want, rep)

kc.complete("subst", 5); rep = kc.get_shell_msg(timeout=30)["content"]
# rlcompleter appends "(" to callables
check("completion", "substitute(" in rep.get("matches", []) and rep["cursor_start"] == 0 and rep["cursor_end"] == 5, rep)

msg_id = kc.execute("import time\ntime.sleep(30)")
time.sleep(2)
km.interrupt_kernel()
t0 = time.time()
reply = kc.get_shell_msg(timeout=30)
while reply["parent_header"].get("msg_id") != msg_id:
    reply = kc.get_shell_msg(timeout=30)
check("interrupt stops a running cell", reply["content"]["status"] == "error" and reply["content"].get("ename") == "KeyboardInterrupt" and time.time() - t0 < 10, reply["content"])
while True:
    m = kc.get_iopub_msg(timeout=30)
    if m["parent_header"].get("msg_id") == msg_id and m["msg_type"] == "status" and m["content"]["execution_state"] == "idle": break
r, out = run("print('after interrupt')")
check("kernel usable after interrupt", r["status"] == "ok" and any(c.get("text") == "after interrupt\n" for t, c in out), (r, out))

kc.stop_channels(); km.shutdown_kernel(now=False)
print("RESULT:", "all passed" if not failures else f"{len(failures)} failed")
sys.exit(1 if failures else 0)
