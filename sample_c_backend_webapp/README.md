Step 2: Install Emscripten (emsdk)
Open PowerShell as Administrator

Press Win + X → Click Windows Terminal (Admin) or PowerShell (Admin)

Clone the Emscripten SDK Repository

sh
Copy
Edit
git clone https://github.com/emscripten-core/emsdk.git
This downloads the Emscripten SDK.

Enter the Emsdk Directory

sh
Copy
Edit
cd emsdk
Download and Install the Latest Version

sh
Copy
Edit
.\emsdk install latest
This command downloads and installs the latest available Emscripten tools.

Activate the Installed SDK

sh
Copy
Edit
.\emsdk activate latest
This sets up the system to use the latest installed Emscripten version.

Enable Emscripten in the Current Terminal

sh
Copy
Edit
.\emsdk_env.bat
This updates your system environment variables for the current session.

Step 3: Verify the Installation
Check if emcc is available:

sh
Copy
Edit
emcc -v
If installed correctly, this will display the Emscripten version.

<!-- Step 4: Make Emscripten Available in Every Terminal
To avoid running .\emsdk_env.bat every time:

Add Emscripten to your System Path

Open PowerShell and run:

sh
Copy
Edit
[System.Environment]::SetEnvironmentVariable("PATH", $Env:PATH + ";" + (Get-Location) + "\emsdk", [System.EnvironmentVariableTarget]::User)
Restart your Terminal or System
After restarting, you should be able to use emcc in any command line. -->

run these inside the git emsdk

.\emsdk activate latest
.\emsdk_env.bat
emcc -v

emcc fluid_sim.c -o fluid_sim.js -s EXPORTED_FUNCTIONS="['_update_simulation', '_get_velocity']" -s EXPORTED_RUNTIME_METHODS="['cwrap']" -s MODULARIZE=1 -s EXPORT_NAME="createModule"

python -m http.server 8000
