# fluid-pendant

A captivating fluid simulation designed to be displayed on an LED panel.

## How to Use

1.  **Start the HTTP Server:** Navigate to the root directory of this project in your terminal and run the following command:
    ```bash
    python -m http.server 8000
    ```
    This will start a simple web server, making the simulation accessible through your web browser.

2.  **View Simulations:**

    * **Normal Simulation and Grid (Side-by-Side):** Open your web browser and go to the following address. Scroll down to see both the regular simulation and its underlying grid representation:
        ```
        http://localhost:8000/flip_simple.html
        ```

    * **Grid-Only Simulation:** To view just the grid-based simulation, open this address in your browser:
        ```
        http://localhost:8000/flip_simple_sim.html
        ```
        Once loaded, you can control the simulation using the following keys:
        * **`p`**: Play the simulation.
        * **`m`**: Pause the simulation.

    * **Gravity Control:** In both simulation views, you can dynamically change the direction of gravity. This will alter the orientation and flow of the fluid. Experiment to see the different effects!

## Folder Structure

* **`flip_sim_js/`**: Contains the same core simulation logic as `flip_simple_sim.html`, but organized into separate JavaScript classes. This structure was intended for potential conversion to C and further optimization by removing unnecessary elements.

* **`flip_sim_c/`**: Holds the C language version of the fluid simulation, which originated from the JavaScript code in `flip_sim_js/`.

* **`sample_c_backend_webapp/`**: This folder represents an initial attempt to automatically convert the JavaScript simulation to C and run it as a web application backend. This approach was not ultimately pursued.

* **`sample_c_UI/`**: Contains components and code designed to run the fluid simulation as a standalone Windows C application with a graphical user interface.

## Additional Information for Mobile Accelerometer Use (Android)

You can also control the simulation's gravity using your mobile device's accelerometer for a more interactive experience. Here's how to set it up:

1.  **Serve `flip_mobile_accelerometer.html` with Ngrok:** First, you need to expose your local server to the internet using Ngrok. If you haven't already, download and install Ngrok. Then, in a new terminal window, run:
    ```bash
    ngrok http 8000
    ```
    Ngrok will provide you with a temporary public URL (e.g., `https://your-ngrok-url.ngrok-free.app`).

2.  **Access on Mobile:** Open a **browser other than Chrome** on your Android device and navigate to the HTTPS URL provided by Ngrok for port 8000 (e.g., `https://your-ngrok-url.ngrok-free.app/flip_mobile_accelerometer.html`). Chrome might have issues with accessing device sensors in this context, so try Firefox or another browser.

3.  **Test Your Phone's Sensors:** If you're unsure whether your phone's sensors are working correctly, you can visit this helpful website in your mobile browser:
    ```
    [https://sensor-js.xyz/demo.html](https://sensor-js.xyz/demo.html)
    ```
    This will allow you to see the data being reported by your device's various sensors.

Now you should be able to tilt and move your phone to influence the fluid simulation running in your mobile browser!