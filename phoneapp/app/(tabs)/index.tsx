import React, { useState, useEffect } from 'react';
import { View, Text, FlatList, TouchableOpacity, PermissionsAndroid, Platform, StyleSheet } from 'react-native';
import { BleManager, Device } from 'react-native-ble-plx';

const bleManager = new BleManager();

export default function BluetoothScreen() {
  const [devices, setDevices] = useState<Device[]>([]);
  const [isScanning, setIsScanning] = useState(false);
  const [connectedDevice, setConnectedDevice] = useState<Device | null>(null);
  const [bleError, setBleError] = useState<string | null>(null);

  useEffect(() => {
    requestPermissions();
    
    // Listen for global Bluetooth state changes (e.g. user turns off Bluetooth)
    const subscription = bleManager.onStateChange((state) => {
      if (state === 'PoweredOff') {
        setBleError('Bluetooth is turned off on your device.');
      } else if (state === 'Unsupported') {
        setBleError('Bluetooth LE is unsupported on this device.');
      } else if (state === 'Unauthorized') {
        setBleError('Bluetooth permissions are not granted.');
      } else if (state === 'PoweredOn') {
        setBleError(null); // Clear error if it turns back on
      }
    }, true); // true means it fires immediately with the current state

    return () => {
      subscription.remove();
      bleManager.destroy();
    };
  }, []);

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
          if (!prev.find(d => d.id === device.id)) {
            return [...prev, device];
          }
          return prev;
        });
      }
    });

    // Stop scanning after 5 seconds
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
      setConnectedDevice(connected);
      setBleError(null);
    } catch (e: any) {
      setBleError(`Connection Failed: ${e.message}`);
    }
  };

  return (
    <View style={styles.container}>
      <Text style={styles.header}>Bluetooth Devices</Text>
      
      {bleError && (
        <View style={styles.errorBadge}>
          <Text style={styles.errorText}>{bleError}</Text>
        </View>
      )}
      
      {connectedDevice ? (
        <View style={styles.connectedContainer}>
          <Text style={styles.connectedText}>Connected to: {connectedDevice.name}</Text>
          <TouchableOpacity 
            style={styles.button}
            onPress={async () => {
              await bleManager.cancelDeviceConnection(connectedDevice.id);
              setConnectedDevice(null);
            }}
          >
            <Text style={styles.buttonText}>Disconnect</Text>
          </TouchableOpacity>
        </View>
      ) : (
        <>
          <TouchableOpacity style={styles.button} onPress={startScan} disabled={isScanning}>
            <Text style={styles.buttonText}>{isScanning ? 'Scanning...' : 'Scan for Devices'}</Text>
          </TouchableOpacity>
          
          <FlatList
            data={devices}
            keyExtractor={(item) => item.id}
            renderItem={({ item }) => (
              <TouchableOpacity style={styles.deviceItem} onPress={() => connectToDevice(item)}>
                <Text style={styles.deviceName}>{item.name}</Text>
                <Text style={styles.deviceId}>{item.id}</Text>
              </TouchableOpacity>
            )}
          />
        </>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, padding: 20, backgroundColor: '#f5f5f5', paddingTop: 60 },
  header: { fontSize: 24, fontWeight: 'bold', marginBottom: 20, textAlign: 'center' },
  button: { backgroundColor: '#007AFF', padding: 15, borderRadius: 8, alignItems: 'center', marginBottom: 20 },
  buttonText: { color: 'white', fontSize: 16, fontWeight: 'bold' },
  deviceItem: { backgroundColor: 'white', padding: 15, borderRadius: 8, marginBottom: 10, shadowColor: '#000', shadowOffset: { width: 0, height: 1 }, shadowOpacity: 0.2, shadowRadius: 1, elevation: 2 },
  deviceName: { fontSize: 16, fontWeight: 'bold' },
  deviceId: { fontSize: 12, color: '#666', marginTop: 4 },
  connectedContainer: { alignItems: 'center', marginTop: 50 },
  connectedText: { fontSize: 18, marginBottom: 20, fontWeight: 'bold', color: 'green' },
  errorBadge: { backgroundColor: '#ffebee', padding: 12, borderRadius: 8, marginBottom: 20, borderWidth: 1, borderColor: '#ffcdd2' },
  errorText: { color: '#c62828', fontSize: 14, textAlign: 'center', fontWeight: '500' }
});
