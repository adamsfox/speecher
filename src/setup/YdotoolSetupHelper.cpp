#include "HelperCommands.h"
#include "YdotoolSetupState.h"
#include "YdotoolSetupTransaction.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

using namespace speecher::helpers;

namespace {

constexpr std::string_view stateFilePath = "/var/lib/speecher/ydotool-setup.json";
constexpr std::string_view modulesLoadPath = "/etc/modules-load.d/speecher-uinput.conf";
constexpr std::string_view udevRulePath = "/etc/udev/rules.d/70-speecher-uinput.rules";
constexpr std::string_view serviceName = "speecher-ydotoold.service";
constexpr std::string_view groupName = "speecher-uinput";

std::string serviceFilePath()
{
    std::error_code error;
    if (std::filesystem::is_directory("/usr/lib/systemd/user", error)) {
        return "/usr/lib/systemd/user/" + std::string(serviceName);
    }
    return "/lib/systemd/user/" + std::string(serviceName);
}

constexpr std::string_view serviceText =
    "[Unit]\n"
    "Description=Speecher virtual keyboard daemon\n"
    "\n"
    "[Service]\n"
    "Type=simple\n"
    "ExecStart=/usr/bin/ydotoold --socket-path=%t/.ydotool_socket --socket-perm=0600\n"
    "Restart=on-failure\n"
    "RestartSec=1\n"
    "\n"
    "[Install]\n"
    "WantedBy=default.target\n";

bool ydotoolInstalled()
{
    return findExecutable("ydotool").has_value() && findExecutable("ydotoold").has_value();
}

bool installYdotoolPackage(std::string &error)
{
    if (ydotoolInstalled()) {
        return true;
    }
    if (findExecutable("apt-get")) {
        return run("apt-get", {"update"}, error)
            && run("apt-get", {"install", "-y", "ydotool"}, error);
    }
    if (findExecutable("dnf")) {
        return run("dnf", {"install", "-y", "ydotool"}, error);
    }
    if (findExecutable("zypper")) {
        return run("zypper", {"--non-interactive", "install", "ydotool"}, error);
    }
    if (findExecutable("pacman")) {
        error = "Install ydotool through a full Arch system upgrade (sudo pacman -Syu ydotool), then run setup again";
        return false;
    }
    error = "No supported package manager found for installing ydotool";
    return false;
}

bool writeState(bool packageWasInstalled, const std::string &user, std::string &error)
{
    return writeFile(std::string(stateFilePath),
                     speecher::ydotoolSetupStateText(
                         packageWasInstalled, serviceFilePath(), user),
                     error);
}

bool install(const std::string &user, std::string &error)
{
    speecher::YdotoolSetupTransaction transaction;
    const auto failed = [&] {
        transaction.appendToError(error);
        return false;
    };
    const bool packageMissingBeforeInstall = !ydotoolInstalled();
    if (!installYdotoolPackage(error)) {
        return failed();
    }
    if (packageMissingBeforeInstall) {
        transaction.record("installed the ydotool package");
    }
    if (!run("modprobe", {"uinput"}, error)) {
        return failed();
    }
    transaction.record("loaded the uinput kernel module");
    if (!writeFile(std::string(modulesLoadPath), "uinput\n", error)) {
        return failed();
    }
    transaction.record("wrote " + std::string(modulesLoadPath));
    bool groupCreated = false;
    if (!ensureGroup(groupName, groupCreated, error)) {
        return failed();
    }
    if (groupCreated) {
        transaction.record("created group " + std::string(groupName));
    }
    bool userAdded = false;
    if (!addUserToGroup(groupName, user, userAdded, error)) {
        return failed();
    }
    if (userAdded) {
        transaction.record("added " + user + " to " + std::string(groupName));
    }
    if (!writeFile(std::string(udevRulePath),
                   "KERNEL==\"uinput\", SUBSYSTEM==\"misc\", OPTIONS+=\"static_node=uinput\", GROUP=\"speecher-uinput\", MODE=\"0660\", TAG+=\"uaccess\"\n",
                   error)) {
        return failed();
    }
    transaction.record("wrote " + std::string(udevRulePath));
    run("udevadm", {"control", "--reload-rules"}, error, true, true);
    run("udevadm", {"trigger", "--subsystem-match=misc", "--attr-match=name=uinput"}, error, true, true);
    const std::string servicePath = serviceFilePath();
    if (!writeFile(servicePath, serviceText, error)) {
        return failed();
    }
    transaction.record("wrote " + servicePath);
    run("systemctl", {"--global", "enable", std::string(serviceName)}, error, true, true);
    if (!writeState(packageMissingBeforeInstall, user, error)) {
        return failed();
    }
    return true;
}

bool remove(const std::string &user, std::string &error)
{
    run("systemctl", {"--global", "disable", std::string(serviceName)}, error, true, true);
    run("gpasswd", {"-d", user, std::string(groupName)}, error, true, true);
    if (!removeFileIfPresent(serviceFilePath(), error)
        || !removeFileIfPresent(std::string(udevRulePath), error)
        || !removeFileIfPresent(std::string(modulesLoadPath), error)
        || !removeFileIfPresent(std::string(stateFilePath), error)) {
        return false;
    }
    run("udevadm", {"control", "--reload-rules"}, error, true, true);
    run("udevadm", {"trigger", "--subsystem-match=misc", "--attr-match=name=uinput"}, error, true, true);
    return true;
}

void printHelp(const char *program)
{
    std::cout << "Usage: " << program << " (--install|--remove) --user USER\n";
}

} // namespace

int main(int argc, char **argv)
{
    std::string user;
    bool doInstall = false;
    bool doRemove = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--help") {
            printHelp(argv[0]);
            return 0;
        }
        if (argument == "--install") {
            doInstall = true;
        } else if (argument == "--remove") {
            doRemove = true;
        } else if (argument == "--user" && index + 1 < argc) {
            user = argv[++index];
        } else {
            std::cerr << "Unknown argument\n";
            return 2;
        }
    }
    if (geteuid() != 0) {
        std::cerr << "This helper must run as root through pkexec\n";
        return 3;
    }
    std::string error;
    if (doInstall == doRemove || !validateUser(user, error)) {
        std::cerr << (error.empty() ? "Choose exactly one action\n" : error + '\n');
        return 2;
    }
    if (!(doInstall ? install(user, error) : remove(user, error))) {
        std::cerr << error << '\n';
        return 1;
    }
    return 0;
}
