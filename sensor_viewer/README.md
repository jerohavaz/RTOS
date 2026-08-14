# sensor_viewer

Small Qt viewer for accelerometer and gyroscope data. It shows the live sensor values and a 3D orientation estimate.

## Run

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python3 sensor_viewer.py /dev/ttyACM0
```

Use `--baud` to change the serial baud rate if needed.

## Orientation estimate

The viewer stores orientation as a **unit quaternion** `(w, x, y, z)`.

Gyroscope angular velocity is integrated into the quaternion each update. Accelerometer data provides the gravity direction and is used as Mahony-style proportional feedback to correct roll and pitch drift. Because there is no magnetometer or other heading reference, absolute yaw cannot be corrected and will slowly drift.

Reference: R. Mahony, T. Hamel, and J.-M. Pflimlin, *Nonlinear Complementary Filters on the Special Orthogonal Group*, IEEE Transactions on Automatic Control, 53(5), 2008. https://doi.org/10.1109/TAC.2008.923738
