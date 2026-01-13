# Acf Shell

A shell environment for running shell scripts embedded in targeted ACF files.
This allows dynamic execution of scripts as part of the ACF workflow, enabling
custom script running using targeted acf.

## D-Bus Interfaces

The service provides objects implementing the `xyz.openbmc_project.TacfShell`
interface. To eliminate dependencies on the `phosphor-dbus-interfaces`
repository, no YAML interface files are used. The `TacfShell` interface supports
the following methods:

```sh
xyz.openbmc_project.TacfShell       interface -         -            -
.active                             method    -         as           -
.cancel                             method    s         b            -
.start                              method    stb       b            -
```
