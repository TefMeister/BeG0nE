# run_hello_xr.ps1 -- kept as a pointer: hello_xr from the OpenXR SDK 1.1.63 quits right after making its swapchains
# on the OpenXR Simulator (with or without our layer, D3D12 and D3D11 alike; 2026-10-26), so the dev PC check uses
# our own test program instead: run_testapp.ps1.
Write-Output "use run_testapp.ps1 (hello_xr does not run on the OpenXR Simulator here)"
