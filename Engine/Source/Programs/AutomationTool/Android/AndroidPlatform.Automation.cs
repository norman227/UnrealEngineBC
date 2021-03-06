﻿// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Net.NetworkInformation;
using System.Threading;
using AutomationTool;
using UnrealBuildTool;

public class AndroidPlatform : Platform
{
	private const int DeployMaxParallelCommands = 6;

	public AndroidPlatform()
		: base(UnrealTargetPlatform.Android)
	{
	}

	private static string GetSONameWithoutArchitecture(ProjectParams Params, string DecoratedExeName)
	{
		return Path.Combine(Path.GetDirectoryName(Params.ProjectGameExeFilename), DecoratedExeName) + ".so";
	}

	private static string GetFinalApkName(ProjectParams Params, string DecoratedExeName, bool bRenameUE4Game, string Architecture, string GPUArchitecture)
	{
		string ProjectDir = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(Params.RawProjectPath)), "Binaries/Android");

		if (Params.Prebuilt)
		{
			ProjectDir = Path.Combine(Params.BaseStageDirectory, "Android");
		}

		// Apk's go to project location, not necessarily where the .so is (content only packages need to output to their directory)
		string ApkName = Path.Combine(ProjectDir, DecoratedExeName) + Architecture + GPUArchitecture + ".apk";

		// if the source binary was UE4Game, handle using it or switching to project name
		if (Path.GetFileNameWithoutExtension(Params.ProjectGameExeFilename) == "UE4Game")
		{
			if (bRenameUE4Game)
			{
				// replace UE4Game with project name (only replace in the filename part)
				ApkName = Path.Combine(Path.GetDirectoryName(ApkName), Path.GetFileName(ApkName).Replace("UE4Game", Params.ShortProjectName));
			}
			else
			{
				// if we want to use UE4 directly then use it from the engine directory not project directory
				ApkName = ApkName.Replace(ProjectDir, Path.Combine(CmdEnv.LocalRoot, "Engine"));
			}
		}

		return ApkName;
	}

	private static string GetFinalObbName(string ApkName)
	{
		// calculate the name for the .obb file
		string PackageName = GetPackageInfo(ApkName, false);
		if (PackageName == null)
		{
            ErrorReporter.Error("Failed to get package name from " + ApkName, (int)ErrorCodes.Error_FailureGettingPackageInfo);
			throw new AutomationException("Failed to get package name from " + ApkName);
		}

		string PackageVersion = GetPackageInfo(ApkName, true);
		if (PackageVersion == null || PackageVersion.Length == 0)
		{
            ErrorReporter.Error("Failed to get package version from " + ApkName, (int)ErrorCodes.Error_FailureGettingPackageInfo);
			throw new AutomationException("Failed to get package version from " + ApkName);
		}

		if (PackageVersion.Length > 0)
		{
			int IntVersion = int.Parse(PackageVersion);
			PackageVersion = IntVersion.ToString("00000");
		}

		string ObbName = string.Format("main.{0}.{1}.obb", PackageVersion, PackageName);

		// plop the .obb right next to the executable
		ObbName = Path.Combine(Path.GetDirectoryName(ApkName), ObbName);

		return ObbName;
	}

	private static string GetDeviceObbName(string ApkName)
	{
		string ObbName = GetFinalObbName(ApkName);
		string PackageName = GetPackageInfo(ApkName, false);
		return "obb/" + PackageName + "/" + Path.GetFileName(ObbName);
	}

    private static string GetStorageQueryCommand()
    {
        return "shell \"echo $EXTERNAL_STORAGE\"";
    }

	private static string GetFinalBatchName(string ApkName, ProjectParams Params, string Architecture, string GPUArchitecture)
	{
		return Path.Combine(Path.GetDirectoryName(ApkName), "Install_" + Params.ShortProjectName + "_" + Params.ClientConfigsToBuild[0].ToString() + Architecture + GPUArchitecture + ".bat");
	}

	public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
	{
		string[] Architectures = UnrealBuildTool.AndroidToolChain.GetAllArchitectures();
		string[] GPUArchitectures = UnrealBuildTool.AndroidToolChain.GetAllGPUArchitectures();
		bool bMakeSeparateApks = UnrealBuildTool.Android.UEDeployAndroid.ShouldMakeSeparateApks();

		foreach (string Architecture in Architectures)
		{
			foreach (string GPUArchitecture in GPUArchitectures)
			{
				string ApkName = GetFinalApkName(Params, SC.StageExecutables[0], true, bMakeSeparateApks ? Architecture : "", bMakeSeparateApks ? GPUArchitecture : "");
				string BatchName = GetFinalBatchName(ApkName, Params, bMakeSeparateApks ? Architecture : "", bMakeSeparateApks ? GPUArchitecture : "");

			// packaging just takes a pak file and makes it the .obb
			UEBuildConfiguration.bOBBinAPK = Params.OBBinAPK; // Make sure this setting is sync'd pre-build
			var Deploy = UEBuildDeploy.GetBuildDeploy(UnrealTargetPlatform.Android);
			if (!Params.Prebuilt)
			{
				string CookFlavor = SC.FinalCookPlatform.IndexOf("_") > 0 ? SC.FinalCookPlatform.Substring(SC.FinalCookPlatform.IndexOf("_")) : "";
				string SOName = GetSONameWithoutArchitecture(Params, SC.StageExecutables[0]);
				Deploy.PrepForUATPackageOrDeploy(Params.ShortProjectName, SC.ProjectRoot, SOName, SC.LocalRoot + "/Engine", Params.Distribution, CookFlavor);
			}

			// first, look for a .pak file in the staged directory
			string[] PakFiles = Directory.GetFiles(SC.StageDirectory, "*.pak", SearchOption.AllDirectories);

			bool bHasPakFile = PakFiles.Length >= 1;

			// for now, we only support 1 pak/obb file
			if (PakFiles.Length > 1)
			{
                string ErrorString = String.Format("Can't package for Android with 0 or more than 1 pak file (found {0} pak files in {1})", PakFiles.Length, SC.StageDirectory);
                ErrorReporter.Error(ErrorString, (int)ErrorCodes.Error_OnlyOneObbFileSupported);
                throw new AutomationException(ErrorString);
			}

			string LocalObbName = GetFinalObbName(ApkName);
			string DeviceObbName = GetDeviceObbName(ApkName);

			// Always delete the target OBB file if it exists
			if (File.Exists(LocalObbName))
			{
				File.Delete(LocalObbName);
			}

			if (!Params.OBBinAPK && bHasPakFile)
			{
				Log("Creating {0} from {1}", LocalObbName, PakFiles[0]);
				File.Copy(PakFiles[0], LocalObbName);
			}

			Log("Writing bat for install with {0}", Params.OBBinAPK ? "OBB in APK" : "OBB separate");
			string PackageName = GetPackageInfo(ApkName, false);
			// make a batch file that can be used to install the .apk and .obb files
			string[] BatchLines = new string[] {
				"setlocal",
				"set ADB=%ANDROID_HOME%\\platform-tools\\adb.exe",
				"set DEVICE=",
				"if not \"%1\"==\"\" set DEVICE=-s %1",
                "for /f \"delims=\" %%A in ('adb " + GetStorageQueryCommand() +"') do @set STORAGE=%%A",
				"%ADB% %DEVICE% uninstall " + PackageName,
				"%ADB% %DEVICE% install " + Path.GetFileName(ApkName),
				"@if \"%ERRORLEVEL%\" NEQ \"0\" goto Error",
				"%ADB% %DEVICE% shell rm -r %STORAGE%/" + Params.ShortProjectName,
				"%ADB% %DEVICE% shell rm -r %STORAGE%/UE4Game/UE4CommandLine.txt", // we need to delete the commandline in UE4Game or it will mess up loading
				"%ADB% %DEVICE% shell rm -r %STORAGE%/obb/" + PackageName,
				Params.OBBinAPK || !bHasPakFile ? "" : "%ADB% %DEVICE% push " + Path.GetFileName(LocalObbName) + " %STORAGE%/" + DeviceObbName,
				Params.OBBinAPK || !bHasPakFile ? "" : "if \"%ERRORLEVEL%\" NEQ \"0\" goto Error",
				"goto:eof",
				":Error",
				"@echo.",
				"@echo There was an error installing the game or the obb file. Look above for more info.",
				"@echo.",
				"@echo Things to try:",
				"@echo Check that the device (and only the device) is listed with \"%ADB$ devices\" from a command prompt.",
				"@echo Make sure all Developer options look normal on the device",
				"@echo Check that the device has an SD card.",
				"@pause"
			};
			File.WriteAllLines(BatchName, BatchLines);
		}
		}

		PrintRunTime();
	}

	public override void GetFilesToArchive(ProjectParams Params, DeploymentContext SC)
	{
		if (SC.StageTargetConfigurations.Count != 1)
		{
            string ErrorString = String.Format("Android is currently only able to package one target configuration at a time, but StageTargetConfigurations contained {0} configurations", SC.StageTargetConfigurations.Count);
            ErrorReporter.Error(ErrorString, (int)ErrorCodes.Error_OnlyOneTargetConfigurationSupported);
			throw new AutomationException(ErrorString);
		}

		string[] Architectures = UnrealBuildTool.AndroidToolChain.GetAllArchitectures();
		string[] GPUArchitectures = UnrealBuildTool.AndroidToolChain.GetAllGPUArchitectures();
		bool bMakeSeparateApks = UnrealBuildTool.Android.UEDeployAndroid.ShouldMakeSeparateApks();

		bool bAddedOBB = false;
		foreach (string Architecture in Architectures)
		{
			foreach (string GPUArchitecture in GPUArchitectures)
			{
				string ApkName = GetFinalApkName(Params, SC.StageExecutables[0], true, bMakeSeparateApks ? Architecture : "", bMakeSeparateApks ? GPUArchitecture : "");
			string ObbName = GetFinalObbName(ApkName);
				string BatchName = GetFinalBatchName(ApkName, Params, bMakeSeparateApks ? Architecture : "", bMakeSeparateApks ? GPUArchitecture : "");

			// verify the files exist
			if (!FileExists(ApkName))
			{
                string ErrorString = String.Format("ARCHIVE FAILED - {0} was not found", ApkName);
                ErrorReporter.Error(ErrorString, (int)ErrorCodes.Error_AppNotFound);
                throw new AutomationException(ErrorString);
			}
			if (!Params.OBBinAPK && !FileExists(ObbName))
			{
                string ErrorString = String.Format("ARCHIVE FAILED - {0} was not found", ObbName);
                ErrorReporter.Error(ErrorString, (int)ErrorCodes.Error_ObbNotFound);
                throw new AutomationException(ErrorString);
            }

			SC.ArchiveFiles(Path.GetDirectoryName(ApkName), Path.GetFileName(ApkName));
			if (!Params.OBBinAPK && !bAddedOBB)
			{
				bAddedOBB = true;
				SC.ArchiveFiles(Path.GetDirectoryName(ObbName), Path.GetFileName(ObbName));
			}

			SC.ArchiveFiles(Path.GetDirectoryName(BatchName), Path.GetFileName(BatchName));
		}
	}
	}

	private string GetAdbCommand(ProjectParams Params)
	{
		string SerialNumber = Params.Device;
		if (SerialNumber.Contains("@"))
		{
			// get the bit after the @
			SerialNumber = SerialNumber.Split("@".ToCharArray())[1];
		}

		if (SerialNumber != "")
		{
			SerialNumber = " -s " + SerialNumber;
		}
		return Environment.ExpandEnvironmentVariables("/c %ANDROID_HOME%/platform-tools/adb.exe" + SerialNumber + " ");
	}

	public override void GetConnectedDevices(ProjectParams Params, out List<string> Devices)
	{
		Devices = new List<string>();
		string AdbCommand = GetAdbCommand(Params);
		ProcessResult Result = Run(CmdEnv.CmdExe, AdbCommand + "devices");

		if (Result.Output.Length > 0)
		{
			string[] LogLines = Result.Output.Split(new char[] { '\n', '\r' });
			bool FoundList = false;
			for (int i = 0; i < LogLines.Length; ++i)
			{
				if (FoundList == false)
				{
					if (LogLines[i].StartsWith("List of devices attached"))
					{
						FoundList = true;
					}
					continue;
				}

				string[] DeviceLine = LogLines[i].Split(new char[] { '\t' });

				if (DeviceLine.Length == 2)
				{
					// the second param should be "device"
					// if it's not setup correctly it might be "unattached" or "powered off" or something like that
					// warning in that case
					if (DeviceLine[1] == "device")
					{
						Devices.Add("@" + DeviceLine[0]);
					}
					else
					{
						CommandUtils.LogWarning("Device attached but in bad state {0}:{1}", DeviceLine[0], DeviceLine[1]);
					}
				}


			}
		}
	}

	/*
	private class TimeRegion : System.IDisposable
	{
		private System.DateTime StartTime { get; set; }

		private string Format { get; set; }

		private System.Collections.Generic.List<object> FormatArgs { get; set; }

		public TimeRegion(string format, params object[] format_args)
		{
			Format = format;
			FormatArgs = new List<object>(format_args);
			StartTime = DateTime.UtcNow;
		}

		public void Dispose()
		{
			double total_time = (DateTime.UtcNow - StartTime).TotalMilliseconds / 1000.0;
			FormatArgs.Insert(0, total_time);
			CommandUtils.Log(Format, FormatArgs.ToArray());
		}
	}
	*/

	internal class LongestFirst : IComparer<string>
	{
		public int Compare(string a, string b)
		{
			if (a.Length == b.Length) return a.CompareTo(b);
			else return b.Length - a.Length;
		}
	}

	public override void Deploy(ProjectParams Params, DeploymentContext SC)
	{
		string DeviceArchitecture = GetBestDeviceArchitecture(Params);
		string GPUArchitecture = "";

		string AdbCommand = GetAdbCommand(Params);
		string ApkName = GetFinalApkName(Params, SC.StageExecutables[0], true, DeviceArchitecture, GPUArchitecture);

		// make sure APK is up to date (this is fast if so)
		var Deploy = UEBuildDeploy.GetBuildDeploy(UnrealTargetPlatform.Android);
		if (!Params.Prebuilt)
		{
			string CookFlavor = SC.FinalCookPlatform.IndexOf("_") > 0 ? SC.FinalCookPlatform.Substring(SC.FinalCookPlatform.IndexOf("_")) : "";
			string SOName = GetSONameWithoutArchitecture(Params, SC.StageExecutables[0]);
			Deploy.PrepForUATPackageOrDeploy(Params.ShortProjectName, SC.ProjectRoot, SOName, SC.LocalRoot + "/Engine", Params.Distribution, CookFlavor);
		}

        // check the APK exists
        if (!File.Exists(ApkName))
        {
            ErrorReporter.Error(String.Format("Could not find apk '{0}'", ApkName), (int)ErrorCodes.Error_AppNotFound);
            throw new AutomationException("Could not find apk '{0}'", ApkName);
        }

		// now we can use the apk to get more info
		string PackageName = GetPackageInfo(ApkName, false);

        // try uninstalling an old app with the same identifier.
        string UninstallCommandline = AdbCommand + "uninstall " + PackageName;
        int SuccessCode = 0;
        RunAndLog(CmdEnv, CmdEnv.CmdExe, UninstallCommandline, out SuccessCode);

		// install the apk
        SuccessCode = 0;
		string InstallCommandline = AdbCommand + "install \"" + ApkName + "\"";
        string InstallOutput = RunAndLog(CmdEnv, CmdEnv.CmdExe, InstallCommandline, out SuccessCode);
        int FailureIndex = InstallOutput.IndexOf("Failure"); 

        // adb install doesn't always return an error code on failure, and instead prints "Failure", followed by an error code.
        if (SuccessCode != 0 || FailureIndex != -1)
        {
            string ErrorMessage = String.Format("Installation of apk '{0}' failed", ApkName);
            if (FailureIndex != -1)
            {
                string FailureString = InstallOutput.Substring(FailureIndex + 7).Trim();
                if (FailureString != "")
                {
                    ErrorMessage += ": " + FailureString;
                }
            }

            ErrorReporter.Error(ErrorMessage, (int)ErrorCodes.Error_AppInstallFailed);
            throw new AutomationException(ErrorMessage);
        }

		// update the ue4commandline.txt
		// update and deploy ue4commandline.txt
		// always delete the existing commandline text file, so it doesn't reuse an old one
		string IntermediateCmdLineFile = CombinePaths(SC.StageDirectory, "UE4CommandLine.txt");
		Project.WriteStageCommandline(IntermediateCmdLineFile, Params, SC);

        // Setup the OBB name and add the storage path (queried from the device) to it
        string DeviceStorageQueryCommand = GetStorageQueryCommand();
        ProcessResult Result = Run(CmdEnv.CmdExe, AdbCommand + DeviceStorageQueryCommand, null, ERunOptions.AppMustExist);
        String StorageLocation = Result.Output.Trim();
        string DeviceObbName = StorageLocation + "/" + GetDeviceObbName(ApkName);

		// copy files to device if we were staging
		if (SC.Stage)
		{
			// cache some strings
			string BaseCommandline = AdbCommand + "push";
			string RemoteDir = StorageLocation + "/" + Params.ShortProjectName;
            string UE4GameRemoteDir = StorageLocation + "/" + Params.ShortProjectName;

			// make sure device is at a clean state
			Run(CmdEnv.CmdExe, AdbCommand + "shell rm -r " + RemoteDir);
			Run(CmdEnv.CmdExe, AdbCommand + "shell rm -r " + UE4GameRemoteDir);

			// Copy UFS files..
			string[] Files = Directory.GetFiles(SC.StageDirectory, "*", SearchOption.AllDirectories);
			System.Array.Sort(Files);

			// Find all the files we exclude from copying. And include
			// the directories we need to individually copy.
			HashSet<string> ExcludedFiles = new HashSet<string>();
			SortedSet<string> IndividualCopyDirectories
				= new SortedSet<string>((IComparer<string>)new LongestFirst());
			foreach (string Filename in Files)
			{
				bool Exclude = false;
				// Don't push the apk, we install it
				Exclude |= Path.GetExtension(Filename).Equals(".apk", StringComparison.InvariantCultureIgnoreCase);
				// For excluded files we add the parent dirs to our
				// tracking of stuff to individually copy.
				if (Exclude)
				{
					ExcludedFiles.Add(Filename);
					// We include all directories up to the stage root in having
					// to individually copy the files.
					for (string FileDirectory = Path.GetDirectoryName(Filename);
						!FileDirectory.Equals(SC.StageDirectory);
						FileDirectory = Path.GetDirectoryName(FileDirectory))
					{
						if (!IndividualCopyDirectories.Contains(FileDirectory))
						{
							IndividualCopyDirectories.Add(FileDirectory);
						}
					}
					if (!IndividualCopyDirectories.Contains(SC.StageDirectory))
					{
						IndividualCopyDirectories.Add(SC.StageDirectory);
					}
				}
			}

			// The directories are sorted above in "deepest" first. We can
			// therefore start copying those individual dirs which will
			// recreate the tree. As the subtrees will get copied at each
			// possible individual level.
			HashSet<string> EntriesToDeploy = new HashSet<string>();
			foreach (string DirectoryName in IndividualCopyDirectories)
			{
				string[] Entries
					= Directory.GetFileSystemEntries(DirectoryName, "*", SearchOption.TopDirectoryOnly);
				foreach (string Entry in Entries)
				{
					// We avoid excluded files and the individual copy dirs
					// (the individual copy dirs will get handled as we iterate).
					if (ExcludedFiles.Contains(Entry) || IndividualCopyDirectories.Contains(Entry))
					{
						continue;
					}
					else
					{
						EntriesToDeploy.Add(Entry);
					}
				}
			}
			if (EntriesToDeploy.Count == 0)
			{
				EntriesToDeploy.Add(SC.StageDirectory);
			}

			// We now have a minimal set of file & dir entries we need
			// to deploy. Files we deploy will get individually copied
			// and dirs will get the tree copies by default (that's
			// what ADB does).
			HashSet<ProcessResult> DeployCommands = new HashSet<ProcessResult>();
			foreach (string Entry in EntriesToDeploy)
			{
				string FinalRemoteDir = RemoteDir;
				string RemotePath = Entry.Replace(SC.StageDirectory, FinalRemoteDir).Replace("\\", "/");
				string Commandline = string.Format("{0} \"{1}\" \"{2}\"", BaseCommandline, Entry, RemotePath);
				// We run deploy commands in parallel to maximize the connection
				// throughput.
				DeployCommands.Add(
					Run(CmdEnv.CmdExe, Commandline, null,
						ERunOptions.Default | ERunOptions.NoWaitForExit));
				// But we limit the parallel commands to avoid overwhelming
				// memory resources.
				if (DeployCommands.Count == DeployMaxParallelCommands)
				{
					while (DeployCommands.Count > DeployMaxParallelCommands / 2)
					{
						Thread.Sleep(10);
						DeployCommands.RemoveWhere(
							delegate(ProcessResult r)
							{
								return r.HasExited;
							});
					}
				}
			}
			foreach (ProcessResult deploy_result in DeployCommands)
			{
				deploy_result.WaitForExit();
			}

			// delete the .obb file, since it will cause nothing we just deployed to be used
			Run(CmdEnv.CmdExe, AdbCommand + "shell rm " + DeviceObbName);
		}
		else if (SC.Archive)
		{
			// deploy the obb if there is one
			string ObbPath = Path.Combine(SC.StageDirectory, GetFinalObbName(ApkName));
			if (File.Exists(ObbPath))
			{
				// cache some strings
				string BaseCommandline = AdbCommand + "push";

				string Commandline = string.Format("{0} \"{1}\" \"{2}\"", BaseCommandline, ObbPath, DeviceObbName);
				Run(CmdEnv.CmdExe, Commandline);
			}
		}
		else
		{
			// cache some strings
			string BaseCommandline = AdbCommand + "push";
            string RemoteDir = StorageLocation + "/" + Params.ShortProjectName;

			string FinalRemoteDir = RemoteDir;
			/*
			// handle the special case of the UE4Commandline.txt when using content only game (UE4Game)
			if (!Params.IsCodeBasedProject)
			{
				FinalRemoteDir = "/mnt/sdcard/UE4Game";
			}
			*/

			string RemoteFilename = IntermediateCmdLineFile.Replace(SC.StageDirectory, FinalRemoteDir).Replace("\\", "/");
			string Commandline = string.Format("{0} \"{1}\" \"{2}\"", BaseCommandline, IntermediateCmdLineFile, RemoteFilename);
			Run(CmdEnv.CmdExe, Commandline);
		}
	}

	/** Internal usage for GetPackageName */
	private static string PackageLine = null;
	private static Mutex PackageInfoMutex = new Mutex();

	/** Run an external exe (and capture the output), given the exe path and the commandline. */
	private static string GetPackageInfo(string ApkName, bool bRetrieveVersionCode)
	{
		// we expect there to be one, so use the first one
		string AaptPath = GetAaptPath();

		PackageInfoMutex.WaitOne();

		var ExeInfo = new ProcessStartInfo(AaptPath, "dump badging \"" + ApkName + "\"");
		ExeInfo.UseShellExecute = false;
		ExeInfo.RedirectStandardOutput = true;
		using (var GameProcess = Process.Start(ExeInfo))
		{
			PackageLine = null;
			GameProcess.BeginOutputReadLine();
			GameProcess.OutputDataReceived += ParsePackageName;
			GameProcess.WaitForExit();
		}

		PackageInfoMutex.ReleaseMutex();

		string ReturnValue = null;
		if (PackageLine != null)
		{
			// the line should look like: package: name='com.epicgames.qagame' versionCode='1' versionName='1.0'
			string[] Tokens = PackageLine.Split("'".ToCharArray());
			int TokenIndex = bRetrieveVersionCode ? 3 : 1;
			if (Tokens.Length >= TokenIndex + 1)
			{
				ReturnValue = Tokens[TokenIndex];
			}
		}
		return ReturnValue;
	}

	/** Simple function to pipe output asynchronously */
	private static void ParsePackageName(object Sender, DataReceivedEventArgs Event)
	{
		// DataReceivedEventHandler is fired with a null string when the output stream is closed.  We don't want to
		// print anything for that event.
		if (!String.IsNullOrEmpty(Event.Data))
		{
			if (PackageLine == null)
			{
				string Line = Event.Data;
				if (Line.StartsWith("package:"))
				{
					PackageLine = Line;
				}
			}
		}
	}

	private static string GetAaptPath()
	{
		// there is a numbered directory in here, hunt it down
		string[] Subdirs = Directory.GetDirectories(Environment.ExpandEnvironmentVariables("%ANDROID_HOME%/build-tools/"));
		if (Subdirs.Length == 0)
		{
            ErrorReporter.Error("Failed to find %ANDROID_HOME%/build-tools subdirectory", (int)ErrorCodes.Error_AndroidBuildToolsPathNotFound);
			throw new AutomationException("Failed to find %ANDROID_HOME%/build-tools subdirectory");
		}
		// we expect there to be one, so use the first one
		return Path.Combine(Subdirs[0], "aapt.exe");
	}

	private string GetBestDeviceArchitecture(ProjectParams Params)
	{
		bool bMakeSeparateApks = UnrealBuildTool.Android.UEDeployAndroid.ShouldMakeSeparateApks();
		// if we are joining all .so's into a single .apk, there's no need to find the best one - there is no other one
		if (!bMakeSeparateApks)
		{
			return "";
		}

		string[] AppArchitectures = AndroidToolChain.GetAllArchitectures();

		// ask the device
		ProcessResult Result = Run(CmdEnv.CmdExe, GetAdbCommand(Params) + " shell getprop ro.product.cpu.abi", null, ERunOptions.AppMustExist);

		// the output is just the architecture
		string DeviceArch = UnrealBuildTool.Android.UEDeployAndroid.GetUE4Arch(Result.Output.Trim());

		// if the architecture wasn't built, look for a backup
		if (Array.IndexOf(AppArchitectures, DeviceArch) == -1)
		{
			// go from 64 to 32-bit
			if (DeviceArch == "-arm64")
			{
				DeviceArch = "-armv7";
			}
			// go from 64 to 32-bit
			else if (DeviceArch == "-x86_64")
			{
				if (Array.IndexOf(AppArchitectures, "-x86") == -1)
				{
					DeviceArch = "-x86";
				}
				// if it didn't have 32-bit x86, look for 64-bit arm for emulation
				// @todo android 64-bit: x86_64 most likely can't emulate arm64 at this ponit
// 				else if (Array.IndexOf(AppArchitectures, "-arm64") == -1)
// 				{
// 					DeviceArch = "-arm64";
// 				}
				// finally try for 32-bit arm emulation (Houdini)
				else
				{
					DeviceArch = "-armv7";
				}
			}
			// use armv7 (with Houdini emulation)
			else if (DeviceArch == "-x86")
			{
				DeviceArch = "-armv7";
			}
		}

		// if after the fallbacks, we still don't have it, we can't continue
		if (Array.IndexOf(AppArchitectures, DeviceArch) == -1)
		{
            string ErrorString = String.Format("Unable to run because you don't have an apk that is usable on {0}", Params.Device);
            ErrorReporter.Error(ErrorString, (int)ErrorCodes.Error_NoApkSuitableForArchitecture);
            throw new AutomationException(ErrorString);
		}

		return DeviceArch;
	}


	public override ProcessResult RunClient(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
	{
		string DeviceArchitecture = GetBestDeviceArchitecture(Params);
		string GPUArchitecture = "";

		string ApkName = ClientApp + DeviceArchitecture + ".apk";
		if (!File.Exists(ApkName))
		{
			ApkName = GetFinalApkName(Params, Path.GetFileNameWithoutExtension(ClientApp), true, DeviceArchitecture, GPUArchitecture);
		}

		Console.WriteLine("Apk='{0}', ClientApp='{1}', ExeName='{2}'", ApkName, ClientApp, Params.ProjectGameExeFilename);

		// run aapt to get the name of the intent
		string PackageName = GetPackageInfo(ApkName, false);
		if (PackageName == null)
		{
            ErrorReporter.Error("Failed to get package name from " + ClientApp, (int)ErrorCodes.Error_FailureGettingPackageInfo);
            throw new AutomationException("Failed to get package name from " + ClientApp);
		}

		string AdbCommand = GetAdbCommand(Params);
		string CommandLine = "shell am start -n " + PackageName + "/com.epicgames.ue4.GameActivity";

		if (Params.Prebuilt)
		{
			// clear the log
			Run(CmdEnv.CmdExe, AdbCommand + "logcat -c");
		}

		// Send a command to unlock the device before we try to run it
		string UnlockCommandLine = "shell input keyevent 82";
		Run(CmdEnv.CmdExe, AdbCommand + UnlockCommandLine, null);

		// start the app on device!
		ProcessResult ClientProcess = Run(CmdEnv.CmdExe, AdbCommand + CommandLine, null, ClientRunFlags);


		if (Params.Prebuilt)
		{
			// save the output to the staging directory
			string LogPath = Path.Combine(Params.BaseStageDirectory, "Android\\logs");
			string LogFilename = Path.Combine(LogPath, "devicelog" + Params.Device + ".log");
			string ServerLogFilename = Path.Combine(CmdEnv.LogFolder, "devicelog" + Params.Device + ".log");

			Directory.CreateDirectory(LogPath);

			// check if the game is still running 
			// time out if it takes to long
			DateTime StartTime = DateTime.Now;
			int TimeOutSeconds = Params.RunTimeoutSeconds;

			while (true)
			{
				ProcessResult ProcessesResult = Run(CmdEnv.CmdExe, AdbCommand + "shell ps", null, ERunOptions.SpewIsVerbose);

				string RunningProcessList = ProcessesResult.Output;
				if (!RunningProcessList.Contains(PackageName))
				{
					break;
				}
				Thread.Sleep(10);

				TimeSpan DeltaRunTime = DateTime.Now - StartTime;
				if ((DeltaRunTime.TotalSeconds > TimeOutSeconds) && (TimeOutSeconds != 0))
				{
					Log("Device: " + Params.Device + " timed out while waiting for run to finish");
					break;
				}
			}

			// this is just to get the ue4 log to go to the output
			Run(CmdEnv.CmdExe, AdbCommand + "logcat -d -s UE4 -s Debug");

			// get the log we actually want to save
			ProcessResult LogFileProcess = Run(CmdEnv.CmdExe, AdbCommand + "logcat -d", null, ERunOptions.AppMustExist);

			File.WriteAllText(LogFilename, LogFileProcess.Output);
			File.WriteAllText(ServerLogFilename, LogFileProcess.Output);
		}


		return ClientProcess;
	}

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
		// 		if (SC.StageExecutables.Count != 1 && Params.Package)
		// 		{
		// 			throw new AutomationException("Exactly one executable expected when staging Android. Had " + SC.StageExecutables.Count.ToString());
		// 		}
		// 
		// 		// stage all built executables
		// 		foreach (var Exe in SC.StageExecutables)
		// 		{
		// 			string ApkName = Exe + GetArchitecture(Params) + ".apk";
		// 
		// 			SC.StageFiles(StagedFileType.NonUFS, Params.ProjectBinariesFolder, ApkName);
		// 		}
	}

	/// <summary>
	/// Gets cook platform name for this platform.
	/// </summary>
	/// <param name="CookFlavor">Additional parameter used to indicate special sub-target platform.</param>
	/// <returns>Cook platform string.</returns>
	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly, string CookFlavor)
	{
		if (CookFlavor.Length > 0)
		{
			return "Android_" + CookFlavor;
		}
		else
		{
			return "Android";
		}
	}

	public override bool DeployPakInternalLowerCaseFilenames()
	{
		return false;
	}

	public override bool DeployLowerCaseFilenames(bool bUFSFile)
	{
		return false;
	}

	public override string LocalPathToTargetPath(string LocalPath, string LocalRoot)
	{
		return LocalPath.Replace("\\", "/").Replace(LocalRoot, "../../..");
	}

	public override bool IsSupported { get { return true; } }

	public override string Remap(string Dest)
	{
		return Dest;
	}

	public override PakType RequiresPak(ProjectParams Params)
	{
		// if packaging is enabled, always create a pak, otherwise use the Params.Pak value
		return Params.Package ? PakType.Always : PakType.DontCare;
	}
    
	#region Hooks

	public override void PostBuildTarget(UE4Build Build, string ProjectName, string UProjectPath, string Config)
	{
		// Run UBT w/ the prep for deployment only option
		// This is required as UBT will 'fake' success when building via UAT and run
		// the deployment prep step before all the required parts are present.
		if (ProjectName.Length > 0)
		{
			string ProjectToBuild = ProjectName;
			if (ProjectToBuild != "UE4Game" && !string.IsNullOrEmpty(UProjectPath))
			{
				ProjectToBuild = UProjectPath;
			}
			string UBTCommand = string.Format("\"{0}\" Android {1} -prepfordeploy", ProjectToBuild, Config);
			CommandUtils.RunUBT(UE4Build.CmdEnv, Build.UBTExecutable, UBTCommand);
		}
	}
	#endregion

	public override List<string> GetDebugFileExtentions()
	{
		return new List<string> { };
	}
}
