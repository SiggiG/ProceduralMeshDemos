git submodule update --init

@echo off
echo Updating submodules
cd Plugins
cd ProceduralMeshes
git checkout master
git pull git@github.com:SiggiG/ProceduralMeshes.git master
cd ..
cd RuntimeMeshComponent
git checkout master
git pull git@github.com:Koderz/UE4RuntimeMeshComponent.git master
pause
