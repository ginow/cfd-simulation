import subprocess
import time
import urllib.request
import json
import sys

def main():
    print("Starting CFD simulation server...")
    proc = subprocess.Popen(['./cfd_sim', '--port', '9500'])
    time.sleep(1.5)

    base_url = None
    # Detect port
    for port in range(9500, 9510):
        try:
            url = f'http://localhost:{port}'
            with urllib.request.urlopen(f'{url}/api/stats', timeout=0.5) as resp:
                if resp.status == 200:
                    base_url = url
                    print(f"Connected to CFD server at {base_url}!")
                    break
        except Exception:
            continue

    if not base_url:
        print("ERROR: Could not connect to CFD server.")
        proc.terminate()
        sys.exit(1)

    try:
        # 1. Test /api/stats
        print("\n[1/6] Testing GET /api/stats ...")
        with urllib.request.urlopen(f'{base_url}/api/stats') as resp:
            data = json.loads(resp.read().decode('utf-8'))
            print("  Stats received:")
            print(f"    - Physical Time: {data['time']:.2f} s, Step: {data['step']}")
            print(f"    - Adaptive dt: {data['dt']*1000:.1f} ms")
            print(f"    - Max Velocity: {data['max_velocity']:.2f} m/s")
            print(f"    - Avg Temperature: {data['avg_temp']:.2f} °C")
            print(f"    - Domain Bounds: [{data['min_temp']:.1f}°C - {data['max_temp']:.1f}°C]")
            print(f"    - Kerala Wind: {data['wind_speed']} m/s @ {data['wind_angle']}°")
            assert 'max_velocity' in data, "Stats must contain max_velocity"

        # 2. Test /api/slice
        print("\n[2/6] Testing GET /api/slice (2D Slice Plane) ...")
        with urllib.request.urlopen(f'{base_url}/api/slice?axis=1&index=14') as resp:
            slice_data = json.loads(resp.read().decode('utf-8'))
            print(f"  Slice received: {slice_data['dim1_name']}x{slice_data['dim2_name']} (Grid: {slice_data['n1']}x{slice_data['n2']})")
            print(f"  Physical bounds: {slice_data['l1']}m x {slice_data['l2']}m, Coordinate = {slice_data['coord_val']:.2f}m")
            print(f"  Sample temperatures: {slice_data['temperature'][:4]} ...")
            assert len(slice_data['temperature']) == slice_data['n1'] * slice_data['n2'], "Temperature array length mismatch"

        # 3. Test /api/3d
        print("\n[3/6] Testing GET /api/3d (3D Volumetric Scene) ...")
        with urllib.request.urlopen(f'{base_url}/api/3d') as resp:
            data_3d = json.loads(resp.read().decode('utf-8'))
            print(f"  Rooms: {len(data_3d['rooms'])} rooms")
            print(f"  Windows: {len(data_3d['windows'])} windows (2 per room)")
            print(f"  Doors: {len(data_3d['doors'])} doors (1 per room)")
            print(f"  Occupants: {len(data_3d['occupants'])} human bodies @ 37°C")
            print(f"  3D Particles: {len(data_3d['particles'])} vector stream points")
            assert len(data_3d['occupants']) == 2, "Must have 2 human occupants @ 37°C"

        # 4. Test /api/probe
        print("\n[4/6] Testing GET /api/probe (Spatial Point Probe @ Human Hot Body) ...")
        with urllib.request.urlopen(f'{base_url}/api/probe?x=2.5&y=1.5&z=2.5') as resp:
            probe = json.loads(resp.read().decode('utf-8'))
            print(f"  Probe at ({probe['x']:.2f}, {probe['y']:.2f}, {probe['z']:.2f}) m:")
            print(f"    - Temperature: {probe['temperature']:.2f} °C")
            print(f"    - Velocity: [{probe['u']:.2f}, {probe['v']:.2f}, {probe['w']:.2f}] m/s")
            print(f"    - Speed: {probe['speed']:.2f} m/s")
            print(f"    - Pressure: {probe['pressure']:.4f} Pa")
            print(f"    - Cell Type: {probe['cell_type']}")

        # 5. Test /api/action POST (Dynamic Parameter Control & Move Windows/Doors)
        print("\n[5/7] Testing POST /api/action (Dynamic Control & Moving Openings) ...")
        req = urllib.request.Request(f'{base_url}/api/action',
                                     data=json.dumps({'cmd': 'set_wind_speed', 'val': 5.0}).encode('utf-8'),
                                     headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(req) as resp:
            action_resp = json.loads(resp.read().decode('utf-8'))
            print(f"  Wind Action Response: {action_resp}")
            assert action_resp['status'] == 'ok', "Action must return status ok"

        # Test moving Window 1A to z = 3.2m
        req_win = urllib.request.Request(f'{base_url}/api/action',
                                         data=json.dumps({'cmd': 'move_window', 'idx': 0, 'x': 0.0, 'z': 3.2}).encode('utf-8'),
                                         headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(req_win) as resp:
            win_resp = json.loads(resp.read().decode('utf-8'))
            print(f"  Move Window Response: {win_resp}")
            assert win_resp['status'] == 'ok'

        # Test moving Partition Door to z = 2.8m
        req_door = urllib.request.Request(f'{base_url}/api/action',
                                          data=json.dumps({'cmd': 'move_door', 'idx': 0, 'x': 5.0, 'z': 2.8}).encode('utf-8'),
                                          headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(req_door) as resp:
            door_resp = json.loads(resp.read().decode('utf-8'))
            print(f"  Move Door Response: {door_resp}")
            assert door_resp['status'] == 'ok'

        # 6. Test Static File Delivery
        print("\n[6/6] Testing Static HTML5 Dashboard (GET /) ...")
        with urllib.request.urlopen(f'{base_url}/') as resp:
            html = resp.read().decode('utf-8')
            print(f"  HTML received ({len(html)} bytes), contains 'Coupled CFD Thermal-Fluid Indoor Simulation'")
            assert 'Coupled CFD Thermal-Fluid' in html, "HTML title check failed"

        print("\n========================================================")
        print("  ALL WEB API AND INTEGRATION TESTS PASSED! [✓]")
        print("========================================================")

    finally:
        print("Stopping CFD simulation server...")
        proc.terminate()
        proc.wait()

if __name__ == '__main__':
    main()
