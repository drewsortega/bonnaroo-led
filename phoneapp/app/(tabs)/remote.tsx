import React, { useState } from 'react';
import { View, Text, TouchableOpacity, StyleSheet } from 'react-native';
import { useBle } from '@/components/BleContext';

export default function RemoteScreen() {
  const { sendCommand } = useBle();
  const [isGameRemote, setIsGameRemote] = useState(false);

  const btn = (label: string, cmd: string, style?: any) => (
    <TouchableOpacity style={[styles.cmdButton, style]} onPress={() => sendCommand(cmd)}>
      <Text style={styles.cmdText}>{label}</Text>
    </TouchableOpacity>
  );

  return (
    <View style={styles.container}>
      <Text style={styles.header}>Remote Control</Text>
      
      <View style={styles.toggleContainer}>
        <TouchableOpacity 
          style={[styles.toggleBtn, !isGameRemote && styles.activeToggle]} 
          onPress={() => setIsGameRemote(false)}
        >
          <Text style={[styles.toggleText, !isGameRemote && styles.activeToggleText]}>Standard</Text>
        </TouchableOpacity>
        <TouchableOpacity 
          style={[styles.toggleBtn, isGameRemote && styles.activeToggle]} 
          onPress={() => setIsGameRemote(true)}
        >
          <Text style={[styles.toggleText, isGameRemote && styles.activeToggleText]}>Game</Text>
        </TouchableOpacity>
      </View>

      <View style={styles.remoteArea}>
        {isGameRemote ? (
          <View style={styles.dpad}>
            <View style={styles.dpadRow}>
              {btn('UP', 'u', styles.dpadBtn)}
            </View>
            <View style={styles.dpadRow}>
              {btn('LEFT', 'l', styles.dpadBtn)}
              {btn('ENTER', 'e', [styles.dpadBtn, styles.enterBtn])}
              {btn('RIGHT', 'r', styles.dpadBtn)}
            </View>
            <View style={styles.dpadRow}>
              {btn('DOWN', 'd', styles.dpadBtn)}
            </View>
          </View>
        ) : (
          <View style={styles.standardRemote}>
            <View style={styles.row}>
              {btn('Brightness -', '-')}
              {btn('Brightness +', '+')}
            </View>
            <View style={styles.row}>
              {btn('< Prev', 'l')}
              {btn('Next >', 'r')}
            </View>
            <View style={styles.row}>
              {btn('MODE / MENU', 'm', styles.menuBtn)}
            </View>
          </View>
        )}
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, padding: 20, backgroundColor: '#f5f5f5', paddingTop: 60 },
  header: { fontSize: 24, fontWeight: 'bold', marginBottom: 20, textAlign: 'center' },
  toggleContainer: { flexDirection: 'row', backgroundColor: '#e0e0e0', borderRadius: 8, padding: 4, marginBottom: 30 },
  toggleBtn: { flex: 1, padding: 12, alignItems: 'center', borderRadius: 6 },
  activeToggle: { backgroundColor: 'white', shadowColor: '#000', shadowOpacity: 0.1, shadowRadius: 2, elevation: 2 },
  toggleText: { fontSize: 16, color: '#666', fontWeight: 'bold' },
  activeToggleText: { color: '#007AFF' },
  remoteArea: { flex: 1, justifyContent: 'center' },
  standardRemote: { gap: 20 },
  row: { flexDirection: 'row', justifyContent: 'space-around', gap: 15 },
  cmdButton: { flex: 1, backgroundColor: '#007AFF', padding: 20, borderRadius: 12, alignItems: 'center', shadowColor: '#000', shadowOpacity: 0.2, shadowRadius: 3, elevation: 3 },
  menuBtn: { backgroundColor: '#5856D6', padding: 25 },
  cmdText: { color: 'white', fontSize: 16, fontWeight: 'bold' },
  dpad: { alignItems: 'center', gap: 10 },
  dpadRow: { flexDirection: 'row', gap: 10 },
  dpadBtn: { width: 80, height: 80, justifyContent: 'center', flex: 0 },
  enterBtn: { backgroundColor: '#FF9500' }
});
