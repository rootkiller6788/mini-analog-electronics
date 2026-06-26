import sys
with open("src/feedback_theorem.lean","w",encoding="utf-8") as f:
    f.write(open("_lean_content.txt","r",encoding="utf-8").read())
print("Done")