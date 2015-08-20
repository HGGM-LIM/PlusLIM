# PlusLIM

Plus is an open-source software toolkit for data acquisition, pre-processing, and calibration for navigated image-guided interventions (https://www.assembla.com/spaces/plus/wiki). This repository is meant as a workspace containing everything Plus related at LIM. Data files (for example Plus-configs and tool definitions) are located in the PlusLIM/Data folder.

## Build Instructions

In, for example, Git Bash run the command:

    git clone https://github.com/HGGM-LIM/PlusLIM/
    
This command will create the PlusLIM folder containing PlusBuild, PlusApp and PlusLib, as well as LIM related additions. Next, before generating the build files using CMake, set **PATH**/PlusLIM/PlusBuild as your source directory and **PATH**/PlusLIM as your build directory. After these parameters are set you can decide on which devices to use (e.g., ConoProbe, OptiTrack, etc).

**OBS!** Because of compatibility issues with older C++ standards it is advised to use a newer Visual Studio version, e.g. VS2013. In case you decide on using VS2013 it might be a good (great?) idea to download the pre-compiled Qt binaries from here: http://sourceforge.net/projects/qt64ng/files/qt/x86-64/4.8.5/msvc2013/
