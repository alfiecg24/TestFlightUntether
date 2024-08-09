# TestFlightUntether

This is a heavily simplified version of the [FSUntether](https://github.com/Ingan121/FSUntether) project. It is a (currently-0day) exploit for iOS 15.0 - 17.0DB1 that launches a basic shell server upon boot. It is not very useful on it's own, but could be combined with a jailbreak to make it untethered, or additional exploits (such as MacDirtyCow) to give it additional capabilities - such as full disk access.

## Usage
Run `make all` in the project directory to generate `TestFlightUntether.ipa`. Install this on your device using an enterprise certificate - it cannot be signed with a regular developer certificate. This is because it must retain the `com.apple.TestFlight` bundle identifier, but using a developer certificate will add your team ID at the end.

Reboot after installing the app. When you reboot, you can access the shell in two ways:
- Use a terminal app such as [a-Shell](https://holzschu.github.io/a-Shell_iOS/) and run `nc localhost 1338`
- Use a computer with your device connected via USB and run `iproxy 1338 1338` and then `nc localhost 1338`

The shell has simple commands, such as `ls`, `cd`, `pwd` and `uname`. It also has a `help` command that lists all available commands.

The original repository has support for full disk access using MacDirtyCow or the CoreTrust bug, but I have not included it in this version.

## How it works
When the device boots, it runs the `TestFlightServiceExtension` binary inside the `TestFlight` bundle, which loads the `TestFlightServices` framework. This framework does not appear to be checked properly, allowing us to replace it with our own dynamic library. This library is loaded into the `TestFlightServiceExtension` process, which is then used to launch the shell server.

## Limitations
- There is, and likely will remain, no way to install this without the original bundle ID
- The shell server is not very useful without additional exploits
