git submodule update --init

@echo off
echo Updating submodules
cd Plugins
cd ProceduralMeshes
git checkout 4.15-VS2015
git pull git@github.com:SiggiG/ProceduralMeshes.git 4.15-VS2015
cd ..
cd RuntimeMeshComponent
git checkout master
git pull git@github.com:Koderz/UE4RuntimeMeshComponent.git master
git checkout 53a885d
pause
