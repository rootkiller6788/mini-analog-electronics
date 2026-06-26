import os
base = r"F:/nano-everything/mini-electronic-info/2. mini-analog-electronics/mini-diode-rectifier"

def writef(relpath, content):
    full = os.path.join(base, relpath.replace("/", os.sep))
    os.makedirs(os.path.dirname(full), exist_ok=True)
    with open(full, "w", encoding="utf-8") as f:
        f.write(content)
    return len(content.splitlines())

total_lines = 0
print("=== Building mini-diode-rectifier ===")
