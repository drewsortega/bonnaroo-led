import React from 'react';
import { View, Text, FlatList, TouchableOpacity, StyleSheet } from 'react-native';
import { useBle } from '@/components/BleContext';

export default function ConnectScreen() {
  const { devices, isScanning, connectedDevice, bleError, startScan, connectToDevice, disconnectDevice } = useBle();

  return (
    <View style={styles.container}>
      <Text style={styles.header}>Bluetooth Connect</Text>
      
      {bleError && (
        <View style={styles.errorBadge}>
          <Text style={styles.errorText}>{bleError}</Text>
        </View>
      )}
      
      {connectedDevice ? (
        <View style={styles.connectedContainer}>
          <Text style={styles.connectedText}>Connected to: {connectedDevice.name}</Text>
          <TouchableOpacity style={styles.button} onPress={disconnectDevice}>
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
