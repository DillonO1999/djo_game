# DJO

## Dependencies

- rayLib

```bash
sudo apt install build-essential git cmake

sudo apt install libasound2-dev libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev libxinerama-dev libwayland-dev libxkbcommon-dev

git clone https://github.com/raysan5/raylib
cd raylib

mkdir build && && cd build
cmake -DBUILD_EXAMPLES=OFF .. 
make
sudo make install
```

## USAGE

In root directory, run: 
```bash
./djo
```