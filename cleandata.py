fnames = ["7mylock1"]

def clean(fname: str) -> str: return f"cleaned_{fname}.csv"

for fname in fnames:
    log = []
    with open(fname) as f:
        log = [int(n) for n in f.read().split(",")]
    print(log)