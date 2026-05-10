import React, { createContext, useContext, useState, useEffect } from 'react';
import { PermissionsAndroid, Platform } from 'react-native';
import { BleManager, Device } from 'react-native-ble-plx';
import { useRouter, useSegments } from 'expo-router';

// Global BleManager instance
export const bleManager = new BleManager();

interface BleContextType {
  connectedDevice: Device | null;
  bleError: string | null;
  isScanning: boolean;
  devices: Device[];
  startScan: () => void;
  connectToDevice: (device: Device) => Promise<void>;
  disconnectDevice: () => Promise<void>;
  sendCommand: (cmd: string) => Promise<void>;
}

const BleContext = createContext<BleContextType | null>(null);

export const useBle = () => {
  const ctx = useContext(BleContext);
  if (!ctx) throw new Error('useBle must be used within BleProvider');
  return ctx;
};

export const BleProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const [devices, setDevices] = useState<Device[]>([]);
  const [isScanning, setIsScanning] = useState(false);
  const [connectedDevice, setConnectedDevice] = useState<Device | null>(null);
  const [bleError, setBleError] = useState<string | null>(null);
  
  const router = useRouter();
  const segments = useSegments();

  useEffect(() => {
    requestPermissions();

    const subscription = bleManager.onStateChange((state) => {
      if (state === 'PoweredOff') setBleError('Bluetooth is turned off on your device.');
      else if (state === 'Unsupported') setBleError('Bluetooth LE is unsupported on this device.');
      else if (state === 'Unauthorized') setBleError('Bluetooth permissions are not granted.');
      else if (state === 'PoweredOn') setBleError(null);
    }, true);

    return () => {
      subscription.remove();
      bleManager.destroy();
    };
  }, []);

  // Force redirect to connect page if not connected
  useEffect(() => {
    const isTabsGroup = segments[0] === '(tabs)';
    if (isTabsGroup && !connectedDevice && segments[1] !== 'connect') {
      router.replace('/(tabs)/connect');
    }
  }, [connectedDevice, segments]);

  const requestPermissions = async () => {
    if (Platform.OS === 'android') {
      await PermissionsAndroid.requestMultiple([
        PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION,
        PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN,
        PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT,
      ]);
    }
  };

  const startScan = () => {
    setBleError(null);
    setDevices([]);
    setIsScanning(true);
    bleManager.startDeviceScan(null, null, (error, device) => {
      if (error) {
        setBleError(`Scan Error: ${error.message}`);
        setIsScanning(false);
        return;
      }
      if (device && device.name) {
        setDevices((prev) => {
          if (!prev.find(d => d.id === device.id)) return [...prev, device];
          return prev;
        });
      }
    });

    setTimeout(() => {
      bleManager.stopDeviceScan();
      setIsScanning(false);
    }, 5000);
  };

  const connectToDevice = async (device: Device) => {
    bleManager.stopDeviceScan();
    setIsScanning(false);
    try {
      const connected = await bleManager.connectToDevice(device.id);
      await connected.discoverAllServicesAndCharacteristics();
      
      // Setup disconnect listener
      connected.onDisconnected((error, disconnectedDevice) => {
        setConnectedDevice(null);
        setBleError('Device disconnected');
      });

      setConnectedDevice(connected);
      setBleError(null);
      // Auto redirect to remote once connected
      router.replace('/(tabs)/remote');
    } catch (e: any) {
      setBleError(`Connection Failed: ${e.message}`);
    }
  };

  const disconnectDevice = async () => {
    if (connectedDevice) {
      await bleManager.cancelDeviceConnection(connectedDevice.id);
      setConnectedDevice(null);
    }
  };

  const sendCommand = async (cmd: string) => {
    if (!connectedDevice) return;
    
    // We need to find the RX characteristic to write to.
    // Standard HM-10 service UUID: 0000ffe0-0000-1000-8000-00805f9b34fb
    // Characteristic UUID: 0000ffe1-0000-1000-8000-00805f9b34fb
    // Many modules vary, so we'll just try to find ANY writable characteristic.
    try {
      const services = await connectedDevice.services();
      for (const service of services) {
        const characteristics = await service.characteristics();
        for (const c of characteristics) {
          if (c.isWritableWithResponse || c.isWritableWithoutResponse) {
            // Encode string to base64
            // React native doesn't have btoa out of the box, we can use a simple helper or just pass base64
            // For now, let's just write. (We will need to add base64 encoding).
          }
        }
      }
    } catch (e) {
      console.warn("Failed to send command", e);
    }
  };

  return (
    <BleContext.Provider value={{
      connectedDevice, bleError, isScanning, devices, startScan, connectToDevice, disconnectDevice, sendCommand
    }}>
      {children}
    </BleContext.Provider>
  );
};
