using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;

static class Program {
    const string GamePath=@"C:\Program Files (x86)\Steam\steamapps\common\Zombies-Arent-Real\zombies-arent-real.exe";
    const string GameSha="E297BC7DB1174EB67116B2B8387B8640554E1BD46598D8D3178091D9DF19B9A9";
    static string TargetPath=GamePath;
    [StructLayout(LayoutKind.Sequential)] struct BootArgs {public uint Size,Mode;public nint Root;}
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] delegate uint Bootstrap(nint args);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate uint NativeTest();
    [DllImport("kernel32.dll",SetLastError=true)] static extern nint OpenProcess(uint access,bool inherit,uint pid);
    [DllImport("kernel32.dll",SetLastError=true)] static extern nint VirtualAllocEx(nint process,nint address,nuint size,uint allocation,uint protect);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool VirtualFreeEx(nint process,nint address,nuint size,uint type);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool WriteProcessMemory(nint process,nint address,byte[] buffer,nuint size,out nuint written);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool ReadProcessMemory(nint process,nint address,byte[] buffer,nuint size,out nuint read);
    [DllImport("kernel32.dll",SetLastError=true)] static extern nint CreateRemoteThread(nint process,nint attributes,nuint stack,nint start,nint parameter,uint flags,out uint tid);
    [DllImport("kernel32.dll",SetLastError=true)] static extern uint WaitForSingleObject(nint handle,uint timeout);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetExitCodeThread(nint thread,out uint exitCode);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(nint handle);
    static void Require(bool ok,string action){if(!ok)throw new Win32Exception(Marshal.GetLastWin32Error(),action);}
    static void VerifyGame(string path){
        if(!string.Equals(Path.GetFullPath(path),TargetPath,StringComparison.OrdinalIgnoreCase))throw new Exception("Target path is not the selected installed game.");
        using var stream=File.OpenRead(path);
        if(Convert.ToHexString(SHA256.HashData(stream))!=GameSha)throw new Exception("Unsupported game executable. No attachment performed; a new build needs analysis.");
    }
    static string Session(string repo,bool runtime=false){
        var directory=Path.Combine(repo,"build","loader","sessions",Guid.NewGuid().ToString("N"));Directory.CreateDirectory(directory);
        var name=runtime?"PIP.Runtime.dll":"PIP.Bootstrap.dll";
        File.Copy(Path.Combine(repo,"build","loader",name),Path.Combine(directory,name));
        File.Copy(Path.Combine(repo,"build","PIP.Core.dll"),Path.Combine(directory,"PIP.Core.dll"));return directory;
    }
    static int SelfTest(string repo){
        var session=Session(repo);var library=NativeLibrary.Load(Path.Combine(session,"PIP.Bootstrap.dll"));
        var start=Marshal.GetDelegateForFunctionPointer<Bootstrap>(NativeLibrary.GetExport(library,"pip_loader_start"));
        var root=Marshal.StringToHGlobalUni(repo);var ptr=Marshal.AllocHGlobal(16);
        try{
            if(start(0)!=1)throw new Exception("Null-argument guard failed.");
            Marshal.StructureToPtr(new BootArgs{Size=0,Mode=1,Root=root},ptr,false);
            if(start(ptr)!=1)throw new Exception("ABI-size guard failed.");
            Marshal.StructureToPtr(new BootArgs{Size=16,Mode=2,Root=root},ptr,false);
            if(start(ptr)!=2)throw new Exception("Unsupported-host guard failed.");
            Marshal.StructureToPtr(new BootArgs{Size=16,Mode=1,Root=root},ptr,false);
            if(start(ptr)!=0)throw new Exception("Native bootstrap/core handshake failed.");
            var log=File.ReadAllText(Path.Combine(repo,"build","PIP.Loader.selftest.log"));
            if(!log.Contains("C ABI version 1 verified."))throw new Exception("Expected self-test log missing.");
            var runtimeSession=Session(repo,true);
            var runtime=NativeLibrary.Load(Path.Combine(runtimeSession,"PIP.Runtime.dll"));
            try{
                var test=Marshal.GetDelegateForFunctionPointer<NativeTest>(NativeLibrary.GetExport(runtime,"pip_runtime_selftest"));
                uint result=test();if(result!=0)throw new Exception($"Runtime navigation/assembly self-test failed at check {result}.");
                Console.WriteLine("PASS: 21 native navigation checks, two-stage retreat with arrival learning, and assembly register/flags replay.");
            }finally{NativeLibrary.Free(runtime);}
            Console.WriteLine("PASS: native bootstrap, null/size/host guards, core loading and ABI handshake. No game attached.");return 0;
        }finally{Marshal.FreeHGlobal(ptr);Marshal.FreeHGlobal(root);NativeLibrary.Free(library);}
    }
    static Process FindGame(){
        var games=Process.GetProcessesByName("zombies-arent-real").Where(p=>string.Equals(p.MainModule?.FileName,TargetPath,StringComparison.OrdinalIgnoreCase)).ToArray();
        if(games.Length!=1)throw new Exception($"Expected one running game; found {games.Length}. Start the game normally in Steam first.");
        var game=games[0];VerifyGame(game.MainModule!.FileName);return game;
    }
    static uint CallThread(nint process,nint start,nint memory,ref bool canFree){
        nint thread=CreateRemoteThread(process,0,0,start,memory,0,out _);Require(thread!=0,"CreateRemoteThread");
        canFree=false;
        try{
            uint wait=WaitForSingleObject(thread,10000);
            if(wait==258)throw new Exception("Loader thread is still running; its context was retained. Check the log before retrying.");
            Require(wait==0,"WaitForSingleObject");canFree=true;
            Require(GetExitCodeThread(thread,out var code),"GetExitCodeThread");return code;
        }finally{CloseHandle(thread);}
    }
    static nint RemoteSystemExport(Process target,string export){
        var system=NativeLibrary.Load(Path.Combine(Environment.SystemDirectory,"kernel32.dll"));
        try{
            var address=NativeLibrary.GetExport(system,export);
            using var current=Process.GetCurrentProcess();
            var owner=current.Modules.Cast<ProcessModule>().Single(m=>address.ToInt64()>=m.BaseAddress.ToInt64()&&address.ToInt64()<m.BaseAddress.ToInt64()+m.ModuleMemorySize);
            var remote=target.Modules.Cast<ProcessModule>().Single(m=>string.Equals(m.FileName,owner.FileName,StringComparison.OrdinalIgnoreCase));
            return remote.BaseAddress+(nint)(address.ToInt64()-owner.BaseAddress.ToInt64());
        }finally{NativeLibrary.Free(system);}
    }
    static void Write(nint process,nint destination,byte[] data){Require(WriteProcessMemory(process,destination,data,(nuint)data.Length,out var n)&&n==(nuint)data.Length,"WriteProcessMemory");}
    static int Attach(string repo,bool runtime=false){
        using var game=FindGame();
        string name=runtime?"PIP.Runtime.dll":"PIP.Bootstrap.dll";
        var existing=game.Modules.Cast<ProcessModule>().SingleOrDefault(m=>string.Equals(m.ModuleName,name,StringComparison.OrdinalIgnoreCase));
        if(existing is not null&&!runtime){Console.WriteLine($"{name} is already loaded.");return 0;}
        if(existing is not null){
            var installedHash=SHA256.HashData(File.ReadAllBytes(existing.FileName));
            var newHash=SHA256.HashData(File.ReadAllBytes(Path.Combine(repo,"build","loader",name)));
            if(!installedHash.SequenceEqual(newHash))throw new Exception("A different PIP runtime build is already loaded. Restart this game instance before enabling the new build.");
        }
        var bootstrapPath=existing?.FileName??Path.Combine(Session(repo,runtime),name);
        var path=Encoding.Unicode.GetBytes(bootstrapPath+'\0');
        var root=Encoding.Unicode.GetBytes(repo+'\0');
        if(path.Length+root.Length+16>4096)throw new Exception("Paths exceed the bootstrap context size.");
        var local=NativeLibrary.Load(bootstrapPath);
        var startRva=NativeLibrary.GetExport(local,"pip_loader_start").ToInt64()-local.ToInt64();
        nint process=OpenProcess(0x43a,false,(uint)game.Id);Require(process!=0,"OpenProcess");
        nint memory=0;bool canFree=true;
        try{
            memory=VirtualAllocEx(process,0,4096,0x3000,4);Require(memory!=0,"VirtualAllocEx");
            if(existing is null){
                var remoteLoad=RemoteSystemExport(game,"LoadLibraryW");
                Write(process,memory,path);
                CallThread(process,remoteLoad,memory,ref canFree);
            }
            game.Refresh();
            var module=game.Modules.Cast<ProcessModule>().SingleOrDefault(m=>string.Equals(m.FileName,bootstrapPath,StringComparison.OrdinalIgnoreCase));
            if(module is null)throw new Exception("Bootstrap was not loaded. No game functions were patched.");
            var context=new byte[16+root.Length];
            BitConverter.GetBytes(16u).CopyTo(context,0);BitConverter.GetBytes(runtime?3u:2u).CopyTo(context,4);
            BitConverter.GetBytes(memory.ToInt64()+16).CopyTo(context,8);root.CopyTo(context,16);
            Write(process,memory,context);
            var result=CallThread(process,module.BaseAddress+(nint)startRva,memory,ref canFree);
            if(result!=0)throw new Exception($"Native initializer returned {result}. Check build/PIP.{(runtime?"Runtime":"Loader")}.log.");
            if(runtime){Console.WriteLine($"Evacuation hooks installed in game PID {game.Id}. Active on the next simulation tick; --disable releases steering.");return 0;}
            Console.WriteLine($"Attached diagnostic loader to game PID {game.Id}; PIP.Core loaded and Odin RTTI verified.");
            Console.WriteLine("No simulation hooks or evacuation behaviour installed. Closing the game unloads both DLLs.");return 0;
        }finally{
            if(memory!=0&&canFree)VirtualFreeEx(process,memory,0,0x8000);
            CloseHandle(process);NativeLibrary.Free(local);
        }
    }
    static int RuntimeControl(bool disable){
        using var game=FindGame();
        var module=game.Modules.Cast<ProcessModule>().SingleOrDefault(m=>string.Equals(m.ModuleName,"PIP.Runtime.dll",StringComparison.OrdinalIgnoreCase));
        if(module is null){Console.WriteLine("PIP runtime is not loaded in this game instance.");return 0;}
        var local=NativeLibrary.Load(module.FileName);
        nint process=OpenProcess(disable?0x43au:0x410u,false,(uint)game.Id);Require(process!=0,"OpenProcess");
        try{
            string export=disable?"pip_runtime_disable":"pip_runtime_stats";
            nint address=module.BaseAddress+(nint)(NativeLibrary.GetExport(local,export).ToInt64()-local.ToInt64());
            if(disable){bool free=true;var code=CallThread(process,address,0,ref free);if(code!=0)throw new Exception($"Disable returned {code}");Console.WriteLine("PIP steering disabled. Hooks remain dormant until the game exits.");}
            else{
                var bytes=new byte[144];Require(ReadProcessMemory(process,address,bytes,(nuint)bytes.Length,out var n)&&n==(nuint)bytes.Length,"ReadProcessMemory");
                if(BitConverter.ToUInt64(bytes,0)!=144)throw new Exception("Loaded runtime uses a different diagnostics layout. Restart the game with this build.");
                string[] fields={"size","enabled","frames","tick","population","regions","breached","portals","nativePaths","steered","arrivals","learned","noRoute","rebuilds","movementObserved","errors","overrides","civilianHook"};
                var values=fields.Select((key,i)=>new KeyValuePair<string,ulong>(key,BitConverter.ToUInt64(bytes,i*8))).ToDictionary();
                Console.WriteLine(System.Text.Json.JsonSerializer.Serialize(values,new System.Text.Json.JsonSerializerOptions{WriteIndented=true}));
            }
            return 0;
        }finally{CloseHandle(process);NativeLibrary.Free(local);}
    }
    public static int Main(string[] args){
        try{
            if(!OperatingSystem.IsWindows()||!Environment.Is64BitProcess)throw new Exception("Windows x64 required.");
            if(args.Length<2||!new[]{"--self-test","--probe","--attach","--enable","--status","--disable"}.Contains(args[0])){
                Console.WriteLine("PIP.Loader --self-test|--probe|--attach|--enable|--status|--disable <repository-folder> [--isolated-test]");
                Console.WriteLine("--enable installs native evacuation hooks in this exact game build. --attach is diagnostic only.");return 0;
            }
            if(args.Contains("--isolated-test"))TargetPath=Path.Combine(Path.GetDirectoryName(GamePath)!,"moddev","pip-native-investigation","playtest","zombies-arent-real.exe");
            var repo=Path.GetFullPath(args[1]);
            if(args[0]=="--self-test")return SelfTest(repo);
            if(args[0]=="--status"||args[0]=="--disable")return RuntimeControl(args[0]=="--disable");
            if(args[0]=="--probe"){using var game=FindGame();Console.WriteLine($"Verified installed game build 26692741, PID {game.Id}; SHA-256 {GameSha}. No attachment performed.");return 0;}
            return Attach(repo,args[0]=="--enable");
        }catch(Exception e){Console.Error.WriteLine(e.Message);return 1;}
    }
}
