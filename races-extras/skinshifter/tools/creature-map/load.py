import re,sqlite3,sys
db=sqlite3.connect('world.db')
def parse_values(s,i):
    # yields tuples from position i
    rows=[];n=len(s)
    while i<n:
        while i<n and s[i] in ' ,\n\r\t': i+=1
        if i>=n or s[i]==';': break
        assert s[i]=='(',s[i:i+50]
        i+=1;row=[]
        while True:
            c=s[i]
            if c=="'":
                j=i+1;buf=[]
                while True:
                    if s[j]=='\\': buf.append(s[j+1]); j+=2; continue
                    if s[j]=="'":
                        if j+1<n and s[j+1]=="'": buf.append("'");j+=2;continue
                        break
                    buf.append(s[j]);j+=1
                row.append(''.join(buf));i=j+1
            else:
                j=i
                while s[j] not in ',)': j+=1
                v=s[i:j].strip()
                row.append(None if v=='NULL' else (float(v) if '.' in v or 'e' in v.lower() else int(v)))
                i=j
            if s[i]==',': i+=1; continue
            if s[i]==')': i+=1; break
        rows.append(row)
    return rows,i
for f in sys.argv[1:]:
    s=open(f,encoding='utf8').read()
    for m in re.finditer(r'CREATE TABLE `(\w+)` \((.*?)\n\)',s,re.S):
        t=m.group(1);cols=re.findall(r'^\s+`(\w+)`',m.group(2),re.M)
        db.execute(f'DROP TABLE IF EXISTS {t}')
        db.execute(f'CREATE TABLE {t} ({",".join(chr(34)+c+chr(34) for c in cols)})')
        cnt=0
        for im in re.finditer(r'INSERT INTO `%s` VALUES\s*'%t,s):
            rows,_=parse_values(s,im.end())
            db.executemany(f'INSERT INTO {t} VALUES ({",".join("?"*len(cols))})',rows);cnt+=len(rows)
        print(t,cnt)
db.commit()
