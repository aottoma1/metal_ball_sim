
# Metal Ball Collision Simulator

OpenGL-based physics simulator using Euler's Integration and varying diffuse coefficients applied on surfaces to demonstrate how different materials absorb kinect energy.


## Dependencies


MacOS
```bash
brew install cmake glfw
```

Linux
```bash
sudo apt install cmake libgl1-mesa-dev libglfw3-dev
```
## Build
```bash
git clone https://github.com/aottoma1/metal_ball_sim.git
cd metal_ball_sim
mkdir build && cd build
cmake ..
make
```
## Textures
To work properly, the textures folder must be copied from the root folder and placed into the newly created build folder.

```bash
cp -r ../textures .
```
## Run
```bash
./MetalBallSim
```
## Controls
- 1 / 2 / 3 - Change Surface Type
- Up / Down - Move Drop Height
- R - Reset Ball Position
## Authors

- [Akash Manghnani](https://www.github.com/AkashManghnani)
- [Andrea Ottomano](https://www.github.com/aottoma1)
- [Gavin Marshall](https://www.github.com/gam0203)
