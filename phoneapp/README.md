# Bonnaroo LED Matrix Bluetooth Controller

This is a React Native app built with Expo to control the Bonnaroo LED matrix via Bluetooth Low Energy (BLE).

Because this app requires native Bluetooth hardware permissions (`react-native-ble-plx`), it **cannot** be run in the standard Expo Go app or inside the iOS Simulator (since the simulator does not support Bluetooth hardware).

To run this app on your computer, you must compile it natively for your Apple Silicon (M-series) Mac. Here is the step-by-step guide to doing that:

## How to Run Natively on your Mac

You need to complete two separate steps to run the app: 
1. Build the native app shell using Xcode.
2. Start the Javascript UI server using Expo.

### Step 1: Build the Native App Shell
1. Open your terminal and navigate to this `phoneapp` directory.
2. Run the following command to open the project in Xcode:
   ```bash
   open ios/phoneapp.xcworkspace
   ```
   *(Make sure you are opening the `.xcworkspace` file, not `.xcodeproj`!)*
3. Once Xcode opens, look at the device selector dropdown at the very top-center of the window (it usually defaults to an iPhone Simulator like "iPhone 15 Pro").
4. Click the dropdown, scroll to the top, and select **"My Mac (Designed for iPad)"**.
5. Click the giant **Play (▶)** button in the top left corner of Xcode.
6. Xcode will compile the native app. Once it finishes, a window for the app will open on your Mac. It will say **"No development servers found."** This is expected!

### Step 2: Start the UI Server
The app you just launched is essentially an empty shell waiting for your Javascript code. We need to start the server that provides that code.
1. Go back to your terminal (inside the `phoneapp` directory).
2. Start the Expo Metro bundler by running:
   ```bash
   npx expo start
   ```
3. Once the server starts running in your terminal, look back at the app window on your Mac. 
4. A development server will magically pop up under the "DEVELOPMENT SERVERS" list. Click on it.
5. The app will load your UI, prompt you for Bluetooth permissions, and you can begin scanning for the Arduino!
