# Windows Build Scripts

* A convenience script for building is provided in `.ci/windows/build.sh`.
* You must run this with Bash, e.g. Git Bash or the MinGW TTY.
* To use this script, you must have `windeployqt` installed (usually bundled with Qt) and set the `WINDEPLOYQT` environment variable to its canonical Bash location:
  * `WINDEPLOYQT="/c/Qt/6.9.1/msvc2022_64/bin/windeployqt6.exe" .ci/windows/build.sh`.
* You can use `aqtinstall`, more info on <https://github.com/miurahr/aqtinstall> and <https://ddalcino.github.io/aqt-list-server/>


* Extra CMake flags should be placed in the arguments of the script.

#### Additional environment variables can be used to control building:

* `BUILD_TYPE` (default `Release`): Sets the build type to use.

* The following environment variables are boolean flags. Set to `true` to enable or `false` to disable:

  * `DEVEL` (default FALSE): Disable Qt update checker
  * `USE_WEBENGINE` (default FALSE): Enable Qt WebEngine
  * `USE_MULTIMEDIA` (default FALSE): Enable Qt Multimedia
  * `BUNDLE_QT` (default FALSE): Use bundled Qt

  * Note that using **system Qt** requires you to include the Qt CMake directory in `CMAKE_PREFIX_PATH`
    * `.ci/windows/build.sh -DCMAKE_PREFIX_PATH=C:/Qt/6.9.0/msvc2022_64/lib/cmake/Qt6`

* After building, a zip can be packaged via `.ci/windows/package.sh`. You must have 7-zip installed and in your PATH.
  * The resulting zip will be placed into `artifacts` in the source directory.


## 🖥️ Method III: CLion Environment Setup

### a. Prerequisites to CLion

* [CLion](https://www.jetbrains.com/clion/) - This IDE is not free; for a free alternative, check Method I

---

### b. Cloning eden with CLion

* Clone the Repository:

<img src="https://user-images.githubusercontent.com/42481638/216899046-0d41d7d6-8e4d-4ed2-9587-b57088af5214.png" width="500">
<img src="https://user-images.githubusercontent.com/42481638/216899061-b2ea274a-e88c-40ae-bf0b-4450b46e9fea.png" width="500">
<img src="https://user-images.githubusercontent.com/42481638/216899076-0e5988c4-d431-4284-a5ff-9ecff973db76.png" width="500">

---

### c. Building & Setup

* Once Cloned, You will be taken to a prompt like the image below:

<img src="https://user-images.githubusercontent.com/42481638/216899092-3fe4cec6-a540-44e3-9e1e-3de9c2fffc2f.png" width="500">

* Set the settings to the image below:
* Change `Build type: Release`
* Change `Name: Release`
* Change `Toolchain Visual Studio`
* Change `Generator: Let CMake decide`
* Change `Build directory: build`

<img src="https://user-images.githubusercontent.com/42481638/216899164-6cee8482-3d59-428f-b1bc-e6dc793c9b20.png" width="500">

* Click OK; now Clion will build a directory and index your code to allow for IntelliSense. Please be patient.
* Once this process has been completed (No loading bar bottom right), you can now build eden
* In the top right, click on the drop-down menu, select all configurations, then select eden

<img src="https://user-images.githubusercontent.com/42481638/216899226-975048e9-bc6d-4ec1-bc2d-bd8a1e15ed04.png" height="500" >

* Now run by clicking the play button or pressing Shift+F10, and eden will auto-launch once built.

<img src="https://user-images.githubusercontent.com/42481638/216899275-d514ec6a-e563-470e-81e2-3e04f0429b68.png" width="500">

---



