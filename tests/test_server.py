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
        print("\n[1/8] Testing GET /api/stats ...")
        with urllib.request.urlopen(f'{base_url}/api/stats') as resp:
            data = json.loads(resp.read().decode('utf-8'))
            print("  Stats received:")
            print(f"    - Physical Time: {data['time']:.2f} s, Step: {data['step']}")
            print(f"    - Adaptive dt: {data['dt']*1000:.1f} ms")
            print(f"    - Max Velocity: {data['max_velocity']:.2f} m/s")
            print(f"    - Avg Temperature: {data['avg_temp']:.2f} °C")
            print(f"    - Domain Bounds: [{data['min_temp']:.1f}°C - {data['max_temp']:.1f}°C]")
            print(f"    - Atmospheric Wind: {data['wind_speed']} m/s @ {data['wind_angle']}°")
            assert 'max_velocity' in data, "Stats must contain max_velocity"

        # 2. Test /api/slice
        print("\n[2/8] Testing GET /api/slice (2D Planar Slice) ...")
        with urllib.request.urlopen(f'{base_url}/api/slice?axis=1&index=14') as resp:
            slice_data = json.loads(resp.read().decode('utf-8'))
            print(f"  Slice received: {slice_data['dim1_name']}x{slice_data['dim2_name']} (Grid: {slice_data['n1']}x{slice_data['n2']})")
            print(f"  Physical bounds: {slice_data['l1']}m x {slice_data['l2']}m, Coordinate = {slice_data['coord_val']:.2f}m")
            print(f"  Sample temperatures: {slice_data['temperature'][:4]} ...")
            assert len(slice_data['temperature']) == slice_data['n1'] * slice_data['n2'], "Temperature array length mismatch"

        # 3. Test /api/3d
        print("\n[3/8] Testing GET /api/3d (3D Volumetric Scene) ...")
        with urllib.request.urlopen(f'{base_url}/api/3d') as resp:
            data_3d = json.loads(resp.read().decode('utf-8'))
            print(f"  Walls: {len(data_3d['walls'])} walls (Fixed 2.80m Height)")
            print(f"  Windows: {len(data_3d['windows'])} windows with casement angles")
            print(f"  Doors: {len(data_3d['doors'])} doors with 90° swing angles")
            print(f"  Occupants: {len(data_3d['occupants'])} 6-foot human bodies @ 37°C")
            print(f"  3D Particles: {len(data_3d['particles'])} vector stream points")
            assert len(data_3d['occupants']) >= 1, "Must have occupants @ 37°C"

        # 4. Test /api/design GET & POST (JSON Export & Import)
        print("\n[4/8] Testing GET & POST /api/design (JSON Export & Import) ...")
        with urllib.request.urlopen(f'{base_url}/api/design') as resp:
            design_json = json.loads(resp.read().decode('utf-8'))
            print(f"  Design exported: {len(design_json['walls'])} walls, {len(design_json['windows'])} windows, {len(design_json['occupants'])} occupants")
            assert 'walls' in design_json and 'domain' in design_json, "Design JSON invalid"

        # Import design back
        req_imp = urllib.request.Request(f'{base_url}/api/design',
                                         data=json.dumps(design_json).encode('utf-8'),
                                         headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(req_imp) as resp:
            imp_resp = json.loads(resp.read().decode('utf-8'))
            print(f"  Design import response: {imp_resp}")
            assert imp_resp['status'] == 'ok', "Design import failed"

        # 5. Test /api/probe
        print("\n[5/8] Testing GET /api/probe (Spatial Point Probe @ 6ft Human Dummy) ...")
        with urllib.request.urlopen(f'{base_url}/api/probe?x=2.5&y=1.5&z=2.5') as resp:
            probe = json.loads(resp.read().decode('utf-8'))
            print(f"  Probe at ({probe['x']:.2f}, {probe['y']:.2f}, {probe['z']:.2f}) m:")
            print(f"    - Temperature: {probe['temperature']:.2f} °C")
            print(f"    - Velocity: [{probe['u']:.2f}, {probe['v']:.2f}, {probe['w']:.2f}] m/s")
            print(f"    - Cell Type: {probe['cell_type']}")

        # 6. Test /api/action POST (CAD Operations: Add Wall, Add Window, Add Door, Add 6ft Dummy)
        print("\n[6/8] Testing POST /api/action (Dynamic CAD Editing Operations) ...")
        # Add Wall
        req_wall = urllib.request.Request(f'{base_url}/api/action',
                                          data=json.dumps({'cmd': 'add_wall', 'x1': 2.0, 'z1': 1.0, 'x2': 2.0, 'z2': 4.0, 'thickness': 0.20}).encode('utf-8'),
                                          headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(req_wall) as resp:
            wall_resp = json.loads(resp.read().decode('utf-8'))
            print(f"  Add Wall Response: {wall_resp}")
            assert wall_resp['status'] == 'ok'

        # Move Window (Vertical elevation + Horizontal)
        req_win = urllib.request.Request(f'{base_url}/api/action',
                                         data=json.dumps({'cmd': 'move_window', 'id': 1, 'x': 0.0, 'y': 1.6, 'z': 3.0}).encode('utf-8'),
                                         headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(req_win) as resp:
            win_resp = json.loads(resp.read().decode('utf-8'))
            print(f"  Move Window (Elevation Y + Z) Response: {win_resp}")
            assert win_resp['status'] == 'ok'

        # Resize Window
        req_res_win = urllib.request.Request(f'{base_url}/api/action',
                                             data=json.dumps({'cmd': 'resize_window', 'id': 1, 'width': 2.0, 'height': 1.4}).encode('utf-8'),
                                             headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(req_res_win) as resp:
            res_win_resp = json.loads(resp.read().decode('utf-8'))
            print(f"  Resize Window Response: {res_win_resp}")
            assert res_win_resp['status'] == 'ok'

        # Move 6ft Occupant
        req_occ = urllib.request.Request(f'{base_url}/api/action',
                                         data=json.dumps({'cmd': 'move_occupant', 'id': 1, 'x': 3.0, 'z': 3.0}).encode('utf-8'),
                                         headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(req_occ) as resp:
            occ_resp = json.loads(resp.read().decode('utf-8'))
            print(f"  Move 6ft Dummy Response: {occ_resp}")
            assert occ_resp['status'] == 'ok'

        # 7. Test Environmental Control (Wind & Outdoor Temperature)
        print("\n[7/8] Testing Environmental Parameters (Universal ABL Profile) ...")
        req_env = urllib.request.Request(f'{base_url}/api/action',
                                         data=json.dumps({'cmd': 'set_wind_speed', 'val': 4.2}).encode('utf-8'),
                                         headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(req_env) as resp:
            env_resp = json.loads(resp.read().decode('utf-8'))
            print(f"  Wind Speed Response: {env_resp}")
            assert env_resp['status'] == 'ok'

        # 8. Test Static HTML5 Dashboard (GET /)
        print("\n[8/8] Testing Static HTML5 Dashboard (GET /) ...")
        with urllib.request.urlopen(f'{base_url}/') as resp:
            html = resp.read().decode('utf-8')
            print(f"  HTML received ({len(html)} bytes), contains 'AeroThermal3D'")
            assert 'AeroThermal3D' in html, "HTML title check failed"

        print("\n========================================================")
        print("  ALL WEB API AND CAD INTEGRATION TESTS PASSED! [✓]")
        print("========================================================")

    finally:
        print("Stopping CFD simulation server...")
        proc.terminate()
        proc.wait()

if __name__ == '__main__':
    main()
