using System.Runtime.InteropServices;

[StructLayout(LayoutKind.Sequential)] struct Point { public float X, Y; public Point(float x,float y){X=x;Y=y;} }
[StructLayout(LayoutKind.Sequential)] struct Region { public ulong Id; public uint Flags, Reserved; }
[StructLayout(LayoutKind.Sequential)] struct Portal {
    public ulong Id, A, B; public Point InsideA, InsideB; public uint Flags; public float Threat;
}
[StructLayout(LayoutKind.Sequential)] struct Person {
    public ulong Id, Region; public Point Position; public uint Flags, Reserved;
}
[StructLayout(LayoutKind.Sequential)] struct State {
    public ulong Known, Revision, FallbackPortal, FallbackRegion, Origin, Target, Portal;
    public double Next;
}
[StructLayout(LayoutKind.Sequential)] struct Decision {
    public uint Action, Reserved; public ulong Target, Portal; public Point Waypoint;
}
[StructLayout(LayoutKind.Sequential)] unsafe struct World {
    public uint Abi, Reserved; public ulong Revision; public double Time;
    public Region* Regions; public uint RegionCount, PortalCount;
    public Portal* Portals; public nint Query, Context; public float Retry, Reserved2;
}
[UnmanagedFunctionPointer(CallingConvention.Cdecl)] unsafe delegate uint Step(World* w, Person* p, State* s, Decision* d);
[UnmanagedFunctionPointer(CallingConvention.Cdecl)] unsafe delegate int Query(nint context, Person* p, ulong portal, ulong target, Point waypoint, float* distance);
[UnmanagedFunctionPointer(CallingConvention.Cdecl)] unsafe delegate void Reset(State* s);
[UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate uint Version();

static unsafe class Program {
    static Step step = null!;
    static Reset reset = null!;
    static readonly Query query = QueryPath;
    static readonly Dictionary<ulong,float> paths = new();
    static int queryCount, checks, scenarios;
    static int QueryPath(nint ctx, Person* p, ulong portal, ulong target, Point waypoint, float* distance) {
        queryCount++;
        if (!paths.TryGetValue(portal,out var d)) return 0;
        *distance=d; return 1;
    }
    static Region R(ulong id,bool breached=false) => new(){Id=id,Flags=breached?3u:1u};
    static Portal P(ulong id,ulong a,ulong b,float threat=0,uint flags=7) => new(){Id=id,A=a,B=b,InsideA=new(10,10),InsideB=new(20,20),Flags=flags,Threat=threat};
    static Person Human(ulong r) => new(){Id=100,Region=r,Position=new(1,1),Flags=3};
    static void Check(bool ok,string message) {checks++; if(!ok)throw new Exception(message);}
    static void Test(string name,Action test) {
        paths.Clear(); paths[11]=20;paths[12]=35;paths[13]=15;queryCount=0;
        test();scenarios++;Console.WriteLine($"PASS {name}");
    }
    static Decision Tick(Region[] regions,Portal[] portals,ref Person person,ref State state,ulong revision=1,double time=0,uint abi=1,float retry=.5f) {
        fixed(Region* rs=regions) fixed(Portal* ps=portals) fixed(Person* p=&person) fixed(State* s=&state) {
            World world=new(){Abi=abi,Revision=revision,Time=time,Regions=rs,RegionCount=(uint)regions.Length,Portals=ps,PortalCount=(uint)portals.Length,Query=Marshal.GetFunctionPointerForDelegate(query),Retry=retry};
            Decision result=default;
            Check(step(&world,p,s,&result)==0,"Unexpected API error"); return result;
        }
    }
    public static int Main(string[] args) {
        try {
            var lib=NativeLibrary.Load(Path.GetFullPath(args[0]));
            step=Marshal.GetDelegateForFunctionPointer<Step>(NativeLibrary.GetExport(lib,"pip_step"));
            reset=Marshal.GetDelegateForFunctionPointer<Reset>(NativeLibrary.GetExport(lib,"pip_reset"));
            var version=Marshal.GetDelegateForFunctionPointer<Version>(NativeLibrary.GetExport(lib,"pip_abi_version"));
            Check(version()==1,"ABI version");
            Check(sizeof(World)==72 && sizeof(State)==64 && sizeof(Decision)==32 && sizeof(Portal)==48 && sizeof(Person)==32,"ABI packing");
            Test("Sheltered residents learn a contingency without moving or querying paths",()=>{
                Person p=Human(1);State s=default;
                var d=Tick([R(1),R(2)],[P(11,1,2)],ref p,ref s);
                Check(d.Action==1&&s.Known==1&&s.FallbackRegion==2&&queryCount==0,"Sheltered plan");
            });
            Test("An outer breach selects the shortest reachable adjacent safe region",()=>{
                Person p=Human(1);State s=default;
                var d=Tick([R(1,true),R(2),R(3)],[P(12,1,3),P(11,1,2)],ref p,ref s);
                Check(d.Action==2&&d.Target==2&&d.Portal==11&&d.Waypoint.X==20,"Adjacent evacuation");
            });
            Test("Arrival teaches the next fallback, enabling repeated inward retreat",()=>{
                Region[] rs=[R(1),R(2),R(3)];Portal[] ps=[P(11,1,2),P(12,2,3)];
                Person p=Human(1);State s=default;Tick(rs,ps,ref p,ref s);
                rs[0]=R(1,true);Check(Tick(rs,ps,ref p,ref s,2).Target==2,"First retreat");
                p.Region=2;Check(Tick(rs,ps,ref p,ref s,2,.1).Action==1&&s.FallbackRegion==3,"Arrival refresh");
                rs[1]=R(2,true);Check(Tick(rs,ps,ref p,ref s,3,.2).Target==3,"Second retreat");
                p.Region=3;Check(Tick(rs,ps,ref p,ref s,3,.3).Action==1&&s.FallbackRegion==0,"Innermost shelter");
            });
            Test("A blocked gate uses another reachable neighbour",()=>{
                paths.Remove(11);Person p=Human(1);State s=default;
                Check(Tick([R(1,true),R(2),R(3)],[P(11,1,2),P(12,1,3)],ref p,ref s).Target==3,"Blocked path");
            });
            Test("Walls and closed or wrong-way portals never become routes",()=>{
                Person p=Human(1);State s=default;
                var d=Tick([R(1,true),R(2),R(3)],[P(11,1,2,0,0),P(12,1,3,0,5)],ref p,ref s);
                Check(d.Action==3&&queryCount==0,"Passability");
            });
            Test("Already breached or inactive destinations are excluded",()=>{
                Person p=Human(1);State s=default;
                var d=Tick([R(1,true),R(2,true),new(){Id=3,Flags=0}],[P(11,1,2),P(12,1,3)],ref p,ref s);
                Check(d.Action==3&&queryCount==0,"Unsafe targets");
            });
            Test("An intact overlap region stays sheltered despite a breached outer region",()=>{
                Person p=Human(2);State s=default;
                Check(Tick([R(1,true),R(2),R(3)],[P(11,1,2),P(12,2,3)],ref p,ref s).Action==1,"Overlap safety");
            });
            Test("Breach of the destination reroutes immediately during the retry window",()=>{
                Region[] rs=[R(1,true),R(2),R(3)];Portal[] ps=[P(11,1,2),P(12,1,3)];Person p=Human(1);State s=default;
                Tick(rs,ps,ref p,ref s);rs[1]=R(2,true);
                Check(Tick(rs,ps,ref p,ref s,2,.1).Target==3,"Destination invalidation");
            });
            Test("Outside transit retains the origin and destination until actual arrival",()=>{
                Person p=Human(1);State s=default;Region[] rs=[R(1,true),R(2)];Portal[] ps=[P(11,1,2)];
                Tick(rs,ps,ref p,ref s);p.Region=0;
                var d=Tick(rs,ps,ref p,ref s,1,.1);Check(d.Action==2&&d.Target==2&&s.Known==0,"Transit");
                p.Region=2;Tick(rs,ps,ref p,ref s,1,.2);Check(s.Known==2&&s.Target==0,"Real arrival");
            });
            Test("With no safe neighbour vanilla panic remains available and retry recovers",()=>{
                Person p=Human(1);State s=default;Region[] rs=[R(1,true),R(2)];Portal[] ps=[P(11,1,2)];
                paths.Clear();Check(Tick(rs,ps,ref p,ref s).Action==3,"No-route decision");
                paths[11]=20;Check(Tick(rs,ps,ref p,ref s,1,.1).Action==3,"Retry bound");
                Check(Tick(rs,ps,ref p,ref s,1,.5).Action==2,"Retry recovery");
            });
            Test("Path closures are rechecked within half a simulation second",()=>{
                Person p=Human(1);State s=default;Region[] rs=[R(1,true),R(2)];Portal[] ps=[P(11,1,2)];
                Tick(rs,ps,ref p,ref s);paths.Clear();
                Check(Tick(rs,ps,ref p,ref s,1,.5).Action==3,"Path closure retry");
            });
            Test("Distant safe regions are never selected across breached intermediate regions",()=>{
                Person p=Human(1);State s=default;
                Check(Tick([R(1,true),R(2,true),R(3)],[P(11,1,2),P(12,2,3)],ref p,ref s).Action==3,"Adjacency");
            });
            Test("Unconnected outside people, soldiers, dead, and infected people stay vanilla",()=>{
                foreach(var pair in new[]{(0UL,3u),(1UL,1u),(1UL,2u),(1UL,7u)}) {
                    Person p=Human(pair.Item1);p.Flags=pair.Item2;State s=default;
                    Check(Tick([R(1,true),R(2)],[P(11,1,2)],ref p,ref s).Action==0,"Excluded person");
                }
            });
            Test("Threat cost influences selection and equal routes use a stable portal ID",()=>{
                Person p=Human(1);State s=default;
                Check(Tick([R(1,true),R(2),R(3)],[P(11,1,2,100),P(12,1,3)],ref p,ref s).Target==3,"Threat score");
                s=default;paths[11]=35;
                Check(Tick([R(1,true),R(2),R(3)],[P(12,1,3),P(11,1,2)],ref p,ref s).Portal==11,"Stable tie");
            });
            Test("Malformed or non-finite path responses are rejected",()=>{
                foreach(float f in new[]{float.NaN,float.PositiveInfinity,-1f}) {
                    Person p=Human(1);State s=default;paths[11]=f;
                    Check(Tick([R(1,true),R(2)],[P(11,1,2)],ref p,ref s).Action==3,"Bad path length");
                }
            });
            Test("Deleted or repaired regions release the override",()=>{
                Person p=Human(1);State s=default;Portal[] ps=[P(11,1,2)];
                Tick([R(1,true),R(2)],ps,ref p,ref s);
                Check(Tick([R(2)],ps,ref p,ref s,2,.1).Action==0&&s.Origin==0,"Deleted origin");
                Tick([R(1,true),R(2)],ps,ref p,ref s,3,.2);
                Check(Tick([R(1),R(2)],ps,ref p,ref s,4,.3).Action==1&&s.Target==0,"Repaired origin");
            });
            Test("Invalid ABI, invalid buffers, invalid times and null pointers fail closed",()=>{
                State s=default;Person p=Human(1);Decision d=default;World w=new(){Abi=999};
                Check(step(&w,&p,&s,&d)==2&&d.Action==0,"ABI mismatch");
                w.Abi=1;w.Retry=.5f;w.RegionCount=1;
                Check(step(&w,&p,&s,&d)==1&&s.Known==0,"Null buffer");
                w.RegionCount=0;w.Time=double.NaN;
                Check(step(&w,&p,&s,&d)==1,"NaN time");
                Check(step(null,&p,&s,&d)==1&&step(&w,&p,&s,null)==1,"Null API pointers");
            });
            Test("Reset erases all session knowledge",()=>{
                State s=new(){Known=1,Revision=9,FallbackPortal=11,FallbackRegion=2,Origin=1,Target=2,Portal=11,Next=20};
                reset(&s);Check(s.Known==0&&s.Revision==0&&s.FallbackPortal==0&&s.FallbackRegion==0&&s.Origin==0&&s.Target==0&&s.Portal==0&&s.Next==0,"Reset");
            });
            Console.WriteLine($"{scenarios} scenarios passed; {checks} assertions. Native x64 DLL exercised through its exported C ABI.");
            Console.WriteLine("This verifies the policy engine, NOT gameplay integration.");
            GC.KeepAlive(query);NativeLibrary.Free(lib);return 0;
        } catch(Exception ex) {Console.Error.WriteLine(ex);return 1;}
    }
}
