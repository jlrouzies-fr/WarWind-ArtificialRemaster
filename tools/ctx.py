import pickle,sys,bisect
from pathlib import Path
lin=pickle.load(open(Path(__file__).resolve().parents[1]/'lin.pkl','rb'))
addrs=[x[0] for x in lin]
before=int(sys.argv[1]); after=int(sys.argv[2])
for t in sys.argv[3:]:
    i=bisect.bisect_left(addrs,int(t,16))
    print('----',t)
    for a,s,m,o in lin[max(0,i-before):i+after+1]: print('%s%x %s %s'%('>' if a==int(t,16) else ' ',a,m,o))
