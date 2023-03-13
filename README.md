# <img src="computer_on_fire.ico" alt="Computer on fire" height="32"/> Computer Status Neo
_WMIC-less rewrite of Computer Status Tool © 2015 Chris Bucher_

WMIC is deprecated as of Windows 10 21H1. With its removal coming at an unknown but inevitable date, he and I decided we need to move Computer Status Tool away from WMIC. And also that it would be better to re-write it from the ground up while we're at it.

**The Lab and Reports tab are not implemented because I don't know of a team that uses them. Let me know if yours does.**

## Advantages
- App doesn't freeze while running a command
- Will continue to work when WMIC goes away
- Dark mode

## New actions
- Reverse Shell
- Reactivate Windows license
- Get Azure AD join status
- List installed printers
- SFC & DISM
- Get serial number
- Get BIOS version
- List network drives
- List physical drives ("HDD or SSD?")
- List installed software

## Missing features
- Lab tab
- Reports tab
- ~~Run as different user~~ working as of v0.2.0!
- Documentation

## Roadmap
See what's in progress on this repo's Project page: https://github.com/the-garlic-os/computer-status-neo/projects/1
