# Camera Tester for Intel Realsense
Quick and dirty test tool run all connected realsense cameras and measure the depth stream frame rate.


### Build
For the camera tester inside the camera_tester folder run:
```
mkdir build && cd build
cmake ..
make
cd ..
```

For the python plot script inside the camera_tester folder run:
```
python3 -m venv venv
source ./venv/bin/activate
pip install -r requirements.txt
```


### Run
To generate logs:
```
./build/camera_tester > camera_fps.log
```

To plot run in terminal with activated venv:
```
./plot_fps.py camera_fps.log
```
which opens a browser window with the a chart.
