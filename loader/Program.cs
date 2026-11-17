using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;

static class Program {
    const string GamePath=@"C:\Program Files (x86)\Steam\steamapps\common\Zombies-Arent-Real\zombies-arent-real.exe";
    const string GameSha="E297BC7DB1174EB67116B2B8387B8640554E1BD46598D8D3178091D9DF19B9A9";
    [StructLayout(LayoutKind.Sequential)] struct BootArgs {public uint Size,Mode;public nint Root;}
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] delegate uint Bootstrap(nint args);
    [DllImport("kernel32.dll",SetLastError=true)] static extern nint OpenProcess(uint access,bool inherit,uint pid);
    [DllImport("kernel32.dll",SetLastError=true)] static extern nint VirtualAllocEx(nint process,nint address,nuint size,uint allocation,uint protect);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool VirtualFreeEx(nint process,nint address,nuint size,uint type);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool WriteProcessMemory(nint process,nint address,byte[] buffer,nuint size,out nuint written);
    [DllImport("kernel32.dll",SetLastError=true)] static extern nint CreateRemoteThread(nint process,nint attributes,nuint stack,nint start,nint parameter,uint flags,out uint tid);
    [DllImport("kernel32.dll",SetLastError=true)] static extern uint WaitForSingleObject(nint handle,uint timeout);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetExitCodeThread(nint thread,out uint exitCode);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(nint handle);
    static void Require(bool ok,string action){if(!ok)throw new Win32Exception(Marshal.GetLastWin32Error(),action);}
    static void VerifyGame(string path){
        if(!string.Equals(Path.GetFullPath(path),GamePath,StringComparison.OrdinalIgnoreCase))throw new Exception("Target path is not the installed game.");
        using var stream=File.OpenRead(path);
        if(Convert.ToHexString(SHA256.HashData(stream))!=GameSha)throw new Exception("Unsupported game executable. No attachment performed; a new build needs analysis.");
    }
    static string Session(string repo){
        var directory=Path.Combine(repo,"build","loader","sessions",Guid.NewGuid().ToString("N"));Directory.CreateDirectory(directory);
        File.Copy(Path.Combine(repo,"build","loader","PIP.Bootstrap.dll"),Path.Combine(directory,"PIP.Bootstrap.dll"));
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
            Console.WriteLine("PASS: native bootstrap, null/size/host guards, core loading and ABI handshake. No game attached.");return 0;
        }finally{Marshal.FreeHGlobal(ptr);Marshal.FreeHGlobal(root);NativeLibrary.Free(library);}
    }
    static Process FindGame(){
        var games=Process.GetProcessesByName("zombies-arent-real");
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
    static int Attach(string repo){
        using var game=FindGame();
        if(game.Modules.Cast<ProcessModule>().Any(m=>string.Equals(m.ModuleName,"PIP.Bootstrap.dll",StringComparison.OrdinalIgnoreCase))){
            Console.WriteLine("PIP bootstrap is already loaded. Check build/PIP.Loader.log. Restart the game to unload it.");return 0;
        }
        var session=Session(repo);var bootstrapPath=Path.Combine(session,"PIP.Bootstrap.dll");
        var path=Encoding.Unicode.GetBytes(bootstrapPath+'\0');
        var root=Encoding.Unicode.GetBytes(repo+'\0');
        if(path.Length+root.Length+16>4096)throw new Exception("Paths exceed the bootstrap context size.");
        var local=NativeLibrary.Load(bootstrapPath);
        var startRva=NativeLibrary.GetExport(local,"pip_loader_start").ToInt64()-local.ToInt64();
        nint process=OpenProcess(0x43a,false,(uint)game.Id);Require(process!=0,"OpenProcess");
        nint memory=0;bool canFree=true;
        try{
            var remoteLoad=RemoteSystemExport(game,"LoadLibraryW");
            memory=VirtualAllocEx(process,0,4096,0x3000,4);Require(memory!=0,"VirtualAllocEx");
            Write(process,memory,path);
            CallThread(process,remoteLoad,memory,ref canFree);
            game.Refresh();
            var module=game.Modules.Cast<ProcessModule>().SingleOrDefault(m=>string.Equals(m.FileName,bootstrapPath,StringComparison.OrdinalIgnoreCase));
            if(module is null)throw new Exception("Bootstrap was not loaded. No game functions were patched.");
            var context=new byte[16+root.Length];
            BitConverter.GetBytes(16u).CopyTo(context,0);BitConverter.GetBytes(2u).CopyTo(context,4);
            BitConverter.GetBytes(memory.ToInt64()+16).CopyTo(context,8);root.CopyTo(context,16);
            Write(process,memory,context);
            var result=CallThread(process,module.BaseAddress+(nint)startRva,memory,ref canFree);
            if(result!=0)throw new Exception($"Bootstrap returned {result}. Check build/PIP.Loader.log; gameplay hooks remain disabled.");
            Console.WriteLine($"Attached diagnostic loader to game PID {game.Id}; PIP.Core loaded and Odin RTTI verified.");
            Console.WriteLine("No simulation hooks or evacuation behaviour installed. Closing the game unloads both DLLs.");return 0;
        }finally{
            if(memory!=0&&canFree)VirtualFreeEx(process,memory,0,0x8000);
            CloseHandle(process);NativeLibrary.Free(local);
        }
    }
    public static int Main(string[] args){
        try{
            if(!OperatingSystem.IsWindows()||!Environment.Is64BitProcess)throw new Exception("Windows x64 required.");
            if(args.Length<2||!new[]{"--self-test","--probe","--attach"}.Contains(args[0])){
                Console.WriteLine("PIP.Loader --self-test|--probe|--attach <repository-folder>");
                Console.WriteLine("Diagnostic prototype. --attach explicitly loads PIP into this exact game build; no gameplay hooks.");return 0;
            }
            var repo=Path.GetFullPath(args[1]);
            if(args[0]=="--self-test")return SelfTest(repo);
            if(args[0]=="--probe"){using var game=FindGame();Console.WriteLine($"Verified installed game build 26692741, PID {game.Id}; SHA-256 {GameSha}. No attachment performed.");return 0;}
            return Attach(repo);
        }catch(Exception e){Console.Error.WriteLine(e.Message);return 1;}
    }
}
