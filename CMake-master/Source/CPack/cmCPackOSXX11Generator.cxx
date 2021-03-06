/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#include "cmCPackOSXX11Generator.h"

#include <sstream>

#include "cm_sys_stat.h"

#include "cmCPackGenerator.h"
#include "cmCPackLog.h"
#include "cmDuration.h"
#include "cmGeneratedFileStream.h"
#include "cmProperty.h"
#include "cmStringAlgorithms.h"
#include "cmSystemTools.h"

cmCPackOSXX11Generator::cmCPackOSXX11Generator() = default;

cmCPackOSXX11Generator::~cmCPackOSXX11Generator() = default;

int cmCPackOSXX11Generator::PackageFiles()
{
  // TODO: Use toplevel ?
  //       It is used! Is this an obsolete comment?

  cmProp cpackPackageExecutables =
    this->GetOption("CPACK_PACKAGE_EXECUTABLES");
  if (cpackPackageExecutables) {
    cmCPackLogger(cmCPackLog::LOG_DEBUG,
                  "The cpackPackageExecutables: " << cpackPackageExecutables
                                                  << "." << std::endl);
    std::ostringstream str;
    std::ostringstream deleteStr;
    std::vector<std::string> cpackPackageExecutablesVector =
      cmExpandedList(cpackPackageExecutables);
    if (cpackPackageExecutablesVector.size() % 2 != 0) {
      cmCPackLogger(
        cmCPackLog::LOG_ERROR,
        "CPACK_PACKAGE_EXECUTABLES should contain pairs of <executable> and "
        "<icon name>."
          << std::endl);
      return 0;
    }
    std::vector<std::string>::iterator it;
    for (it = cpackPackageExecutablesVector.begin();
         it != cpackPackageExecutablesVector.end(); ++it) {
      std::string cpackExecutableName = *it;
      ++it;
      this->SetOptionIfNotSet("CPACK_EXECUTABLE_NAME", cpackExecutableName);
    }
  }

  // Disk image directories
  std::string diskImageDirectory = toplevel;
  std::string diskImageBackgroundImageDir =
    diskImageDirectory + "/.background";

  // App bundle directories
  std::string packageDirFileName = cmStrCat(
    toplevel, '/', this->GetOption("CPACK_PACKAGE_FILE_NAME"), ".app");
  std::string contentsDirectory = packageDirFileName + "/Contents";
  std::string resourcesDirectory = contentsDirectory + "/Resources";
  std::string appDirectory = contentsDirectory + "/MacOS";
  std::string scriptDirectory = resourcesDirectory + "/Scripts";
  std::string resourceFileName =
    cmStrCat(this->GetOption("CPACK_PACKAGE_FILE_NAME"), ".rsrc");

  const char* dir = resourcesDirectory.c_str();
  const char* appdir = appDirectory.c_str();
  const char* scrDir = scriptDirectory.c_str();
  const char* contDir = contentsDirectory.c_str();
  const char* rsrcFile = resourceFileName.c_str();
  cmProp iconFile = this->GetOption("CPACK_PACKAGE_ICON");
  if (iconFile) {
    std::string iconFileName = cmsys::SystemTools::GetFilenameName(iconFile);
    if (!cmSystemTools::FileExists(iconFile)) {
      cmCPackLogger(cmCPackLog::LOG_ERROR,
                    "Cannot find icon file: "
                      << iconFile
                      << ". Please check CPACK_PACKAGE_ICON setting."
                      << std::endl);
      return 0;
    }
    std::string destFileName = resourcesDirectory + "/" + iconFileName;
    this->ConfigureFile(iconFile, destFileName, true);
    this->SetOptionIfNotSet("CPACK_APPLE_GUI_ICON", iconFileName);
  }

  std::string applicationsLinkName = diskImageDirectory + "/Applications";
  cmSystemTools::CreateSymlink("/Applications", applicationsLinkName);

  if (!this->CopyResourcePlistFile("VolumeIcon.icns", diskImageDirectory,
                                   ".VolumeIcon.icns", true) ||
      !this->CopyResourcePlistFile("DS_Store", diskImageDirectory, ".DS_Store",
                                   true) ||
      !this->CopyResourcePlistFile("background.png",
                                   diskImageBackgroundImageDir,
                                   "background.png", true) ||
      !this->CopyResourcePlistFile("RuntimeScript", dir) ||
      !this->CopyResourcePlistFile("OSXX11.Info.plist", contDir,
                                   "Info.plist") ||
      !this->CopyResourcePlistFile("OSXX11.main.scpt", scrDir, "main.scpt",
                                   true) ||
      !this->CopyResourcePlistFile("OSXScriptLauncher.rsrc", dir, rsrcFile,
                                   true) ||
      !this->CopyResourcePlistFile(
        "OSXScriptLauncher", appdir,
        this->GetOption("CPACK_PACKAGE_FILE_NAME").GetCStr(), true)) {
    cmCPackLogger(cmCPackLog::LOG_ERROR,
                  "Problem copying the resource files" << std::endl);
    return 0;
  }

  // Two of the files need to have execute permission, so ensure they do:
  std::string runTimeScript = cmStrCat(dir, "/RuntimeScript");

  std::string appScriptName =
    cmStrCat(appdir, '/', this->GetOption("CPACK_PACKAGE_FILE_NAME"));

  mode_t mode;
  if (cmsys::SystemTools::GetPermissions(runTimeScript.c_str(), mode)) {
    mode |= (S_IXUSR | S_IXGRP | S_IXOTH);
    cmsys::SystemTools::SetPermissions(runTimeScript.c_str(), mode);
    cmCPackLogger(cmCPackLog::LOG_OUTPUT,
                  "Setting: " << runTimeScript << " to permission: " << mode
                              << std::endl);
  }

  if (cmsys::SystemTools::GetPermissions(appScriptName.c_str(), mode)) {
    mode |= (S_IXUSR | S_IXGRP | S_IXOTH);
    cmsys::SystemTools::SetPermissions(appScriptName.c_str(), mode);
    cmCPackLogger(cmCPackLog::LOG_OUTPUT,
                  "Setting: " << appScriptName << " to permission: " << mode
                              << std::endl);
  }

  std::string output;
  std::string tmpFile = cmStrCat(this->GetOption("CPACK_TOPLEVEL_DIRECTORY"),
                                 "/hdiutilOutput.log");
  std::ostringstream dmgCmd;
  dmgCmd << "\"" << this->GetOption("CPACK_INSTALLER_PROGRAM_DISK_IMAGE")
         << "\" create -ov -fs HFS+ -format UDZO -srcfolder \""
         << diskImageDirectory << "\" \"" << packageFileNames[0] << "\"";
  cmCPackLogger(cmCPackLog::LOG_VERBOSE,
                "Compress disk image using command: " << dmgCmd.str()
                                                      << std::endl);
  // since we get random dashboard failures with this one
  // try running it more than once
  int retVal = 1;
  int numTries = 10;
  bool res = false;
  while (numTries > 0) {
    res = cmSystemTools::RunSingleCommand(
      dmgCmd.str(), &output, &output, &retVal, nullptr, this->GeneratorVerbose,
      cmDuration::zero());
    if (res && !retVal) {
      numTries = -1;
      break;
    }
    cmSystemTools::Delay(500);
    numTries--;
  }
  if (!res || retVal) {
    cmGeneratedFileStream ofs(tmpFile);
    ofs << "# Run command: " << dmgCmd.str() << std::endl
        << "# Output:" << std::endl
        << output << std::endl;
    cmCPackLogger(cmCPackLog::LOG_ERROR,
                  "Problem running hdiutil command: "
                    << dmgCmd.str() << std::endl
                    << "Please check " << tmpFile << " for errors"
                    << std::endl);
    return 0;
  }

  return 1;
}

int cmCPackOSXX11Generator::InitializeInternal()
{
  cmCPackLogger(cmCPackLog::LOG_WARNING,
                "The OSXX11 generator is deprecated "
                "and will be removed in a future version.\n");
  cmCPackLogger(cmCPackLog::LOG_DEBUG,
                "cmCPackOSXX11Generator::Initialize()" << std::endl);
  std::vector<std::string> path;
  std::string pkgPath = cmSystemTools::FindProgram("hdiutil", path, false);
  if (pkgPath.empty()) {
    cmCPackLogger(cmCPackLog::LOG_ERROR,
                  "Cannot find hdiutil compiler" << std::endl);
    return 0;
  }
  this->SetOptionIfNotSet("CPACK_INSTALLER_PROGRAM_DISK_IMAGE", pkgPath);

  return this->Superclass::InitializeInternal();
}

/*
bool cmCPackOSXX11Generator::CopyCreateResourceFile(const std::string& name)
{
  std::string uname = cmSystemTools::UpperCase(name);
  std::string cpackVar = "CPACK_RESOURCE_FILE_" + uname;
  const char* inFileName = this->GetOption(cpackVar.c_str());
  if ( !inFileName )
    {
    cmCPackLogger(cmCPackLog::LOG_ERROR, "CPack option: " << cpackVar.c_str()
                  << " not specified. It should point to "
                  << (name ? name : "(NULL)")
                  << ".rtf, " << name
                  << ".html, or " << name << ".txt file" << std::endl);
    return false;
    }
  if ( !cmSystemTools::FileExists(inFileName) )
    {
    cmCPackLogger(cmCPackLog::LOG_ERROR, "Cannot find "
                  << (name ? name : "(NULL)")
                  << " resource file: " << inFileName << std::endl);
    return false;
    }
  std::string ext = cmSystemTools::GetFilenameLastExtension(inFileName);
  if ( ext != ".rtfd" && ext != ".rtf" && ext != ".html" && ext != ".txt" )
    {
    cmCPackLogger(cmCPackLog::LOG_ERROR, "Bad file extension specified: "
      << ext << ". Currently only .rtfd, .rtf, .html, and .txt files allowed."
      << std::endl);
    return false;
    }

  std::string destFileName = cmStrCat(
this->GetOption("CPACK_TOPLEVEL_DIRECTORY"), "/Resources/", name, ext );


  cmCPackLogger(cmCPackLog::LOG_VERBOSE, "Configure file: "
                << (inFileName ? inFileName : "(NULL)")
                << " to " << destFileName << std::endl);
  this->ConfigureFile(inFileName, destFileName);
  return true;
}
*/

bool cmCPackOSXX11Generator::CopyResourcePlistFile(
  const std::string& name, const std::string& dir,
  const char* outputFileName /* = 0 */, bool copyOnly /* = false */)
{
  std::string inFName = cmStrCat("CPack.", name, ".in");
  std::string inFileName = this->FindTemplate(inFName.c_str());
  if (inFileName.empty()) {
    cmCPackLogger(cmCPackLog::LOG_ERROR,
                  "Cannot find input file: " << inFName << std::endl);
    return false;
  }

  if (!outputFileName) {
    outputFileName = name.c_str();
  }

  std::string destFileName = cmStrCat(dir, '/', outputFileName);

  cmCPackLogger(cmCPackLog::LOG_VERBOSE,
                "Configure file: " << inFileName << " to " << destFileName
                                   << std::endl);
  this->ConfigureFile(inFileName, destFileName, copyOnly);
  return true;
}

const char* cmCPackOSXX11Generator::GetPackagingInstallPrefix()
{
  this->InstallPrefix =
    cmStrCat('/', this->GetOption("CPACK_PACKAGE_FILE_NAME"),
             ".app/Contents/Resources");
  return this->InstallPrefix.c_str();
}
