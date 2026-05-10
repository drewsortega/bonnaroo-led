import React, { useState } from 'react';
import { View, Text, TouchableOpacity, StyleSheet, Image } from 'react-native';

export default function UploadScreen() {
  const [selectedImage, setSelectedImage] = useState<string | null>(null);
  const [errorMsg, setErrorMsg] = useState<string | null>(null);

  const mockSelectGif = () => {
    // In the future, use expo-image-picker to select a file.
    // For now, we mock the UI.
    
    // Simulating selecting a valid vs invalid file randomly
    const isInvalid = Math.random() > 0.5;
    
    if (isInvalid) {
      setErrorMsg("Invalid file. Please select a GIF that is exactly 64x64 pixels.");
      setSelectedImage(null);
    } else {
      setErrorMsg(null);
      // Placeholders
      setSelectedImage("https://via.placeholder.com/64x64.gif?text=GIF");
    }
  };

  const uploadGif = () => {
    if (!selectedImage) return;
    alert("Upload functionality coming soon via Bluetooth!");
  };

  return (
    <View style={styles.container}>
      <Text style={styles.header}>Upload GIF</Text>
      
      <View style={styles.card}>
        <Text style={styles.instructions}>
          Select a GIF from your device to upload to the Bonnaroo LED Matrix SD card. 
          {"\n\n"}
          Requirements:
          {"\n"}- Must be a .gif file
          {"\n"}- Must be exactly 64 x 64 pixels
        </Text>

        {errorMsg && (
          <View style={styles.errorBox}>
            <Text style={styles.errorText}>{errorMsg}</Text>
          </View>
        )}

        {selectedImage && (
          <View style={styles.previewContainer}>
            <Text style={styles.previewText}>Selected File:</Text>
            <Image source={{ uri: selectedImage }} style={styles.previewImage} />
            <Text style={styles.validText}>✓ 64x64 GIF Validated</Text>
          </View>
        )}

        <TouchableOpacity style={styles.selectBtn} onPress={mockSelectGif}>
          <Text style={styles.selectBtnText}>Select GIF File</Text>
        </TouchableOpacity>

        <TouchableOpacity 
          style={[styles.uploadBtn, !selectedImage && styles.uploadBtnDisabled]} 
          onPress={uploadGif}
          disabled={!selectedImage}
        >
          <Text style={styles.uploadBtnText}>Upload via Bluetooth</Text>
        </TouchableOpacity>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, padding: 20, backgroundColor: '#f5f5f5', paddingTop: 60 },
  header: { fontSize: 24, fontWeight: 'bold', marginBottom: 20, textAlign: 'center' },
  card: { backgroundColor: 'white', padding: 20, borderRadius: 12, shadowColor: '#000', shadowOpacity: 0.1, shadowRadius: 5, elevation: 3 },
  instructions: { fontSize: 14, color: '#444', lineHeight: 20, marginBottom: 20 },
  errorBox: { backgroundColor: '#ffebee', padding: 10, borderRadius: 6, marginBottom: 15 },
  errorText: { color: '#c62828', fontSize: 13, textAlign: 'center' },
  previewContainer: { alignItems: 'center', marginBottom: 20, padding: 15, backgroundColor: '#f9f9f9', borderRadius: 8 },
  previewText: { fontSize: 14, color: '#666', marginBottom: 10 },
  previewImage: { width: 64, height: 64, backgroundColor: '#ccc' },
  validText: { color: 'green', fontSize: 12, marginTop: 10, fontWeight: 'bold' },
  selectBtn: { backgroundColor: '#E5E5EA', padding: 15, borderRadius: 8, alignItems: 'center', marginBottom: 10 },
  selectBtnText: { color: '#007AFF', fontSize: 16, fontWeight: 'bold' },
  uploadBtn: { backgroundColor: '#34C759', padding: 15, borderRadius: 8, alignItems: 'center' },
  uploadBtnDisabled: { backgroundColor: '#A1E6B2' },
  uploadBtnText: { color: 'white', fontSize: 16, fontWeight: 'bold' }
});
