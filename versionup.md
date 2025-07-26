How to increase version number
===
[versionup.ps1](versionup.ps1) helps to bump the new version number. Following files would be updated automatically.

* [version.h](version.h)
    * This header is used in resource file - it affects property of both CLI and DLL
* [installer/Product.wxs](installer/Product.wxs)
    * Product version

# Usage

```
versionup.ps1 [version number]
```

* Version number should be in 3 or 4 digits (x.y.z or w.x.y.z)
* Version number is converted into [SemVer](https://semver.org/)
    * x.y.z is given, it is used straightly as SemVer, and adds `0` as a revision number in Windows
    * w.x.y.z is given, it is converted into w.x.t+z
