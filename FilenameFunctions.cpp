/*
 * Animated GIFs Display Code for SmartMatrix and 32x32 RGB LED Panels
 *
 * This file contains code to enumerate and select animated GIF files by name
 *
 * Written by: Craig A. Lindley
 */

#include "FilenameFunctions.h"

#include <SD.h>
#include <SPI.h>

// Global file handle shared across all translation units
File my_sd_file;

int numberOfFiles;

bool fileSeekCallback(unsigned long position) {
    return my_sd_file.seek(position);
}

unsigned long filePositionCallback(void) {
    return my_sd_file.position();
}

int fileReadCallback(void) {
    return my_sd_file.read();
}

int fileReadBlockCallback(void * buffer, int numberOfBytes) {
    return my_sd_file.read((uint8_t*)buffer, numberOfBytes);
}

int fileSizeCallback(void) {
    return my_sd_file.size();
}

bool initSDCard(int chipSelectPin, bool use_spi1) {
    // Override and use SPI1 instead of default SPI.
    if (use_spi1) {
        if (!SD.sdfs.begin(SdSpiConfig(chipSelectPin, SHARED_SPI, SPI_HALF_SPEED, &SPI1))) {
            return false;
        }
        return true;
    }

    if (!SD.begin(chipSelectPin)) {
        return false;
    }
    return true;
}

bool isAnimationFile(const char filename []) {
    String filenameString(filename);

#if defined(ESP32)
    // ESP32 filename includes the full path, so need to remove the path before looking at the filename
    int pathindex = filenameString.lastIndexOf("/");
    if(pathindex >= 0)
        filenameString.remove(0, pathindex + 1);
#endif

    if ((filenameString[0] == '_') || (filenameString[0] == '~') || (filenameString[0] == '.')) {
        return false;
    }

    filenameString.toUpperCase();
    if (filenameString.endsWith(".GIF") != 1)
        return false;

    return true;
}

const int MAX_GIFS = 256;
String cached_gif_filenames[MAX_GIFS];
int cached_gif_count = -1;

void invalidateGIFCache() {
    cached_gif_count = -1;
}

// Enumerate and possibly display the animated GIF filenames in GIFS directory
int enumerateGIFFiles(const char *directoryName, bool displayFilenames) {
    if (cached_gif_count >= 0) {
        return cached_gif_count; // Return cached count if valid
    }

    numberOfFiles = 0;
    File directory = SD.open(directoryName);
    File file;

    if (!directory) {
        return -1;
    }

    while (file = directory.openNextFile()) {
        if (isAnimationFile(file.name())) {
            if (numberOfFiles < MAX_GIFS) {
                cached_gif_filenames[numberOfFiles] = file.name();
            }
            numberOfFiles++;
        }
        file.close();
    }
    directory.close();

    // Limit to MAX_GIFS for sorting
    int sort_count = (numberOfFiles > MAX_GIFS) ? MAX_GIFS : numberOfFiles;
    
    // Simple Bubble Sort for alphabetical ordering
    for (int i = 0; i < sort_count - 1; i++) {
        for (int j = i + 1; j < sort_count; j++) {
            if (cached_gif_filenames[i].compareTo(cached_gif_filenames[j]) > 0) {
                String temp = cached_gif_filenames[i];
                cached_gif_filenames[i] = cached_gif_filenames[j];
                cached_gif_filenames[j] = temp;
            }
        }
    }
    
    cached_gif_count = sort_count;
    numberOfFiles = sort_count; // Keep global in sync

    if (displayFilenames) {
        for (int i = 0; i < cached_gif_count; i++) {
            Serial.print(i + 1);
            Serial.print(":");
            Serial.println(cached_gif_filenames[i]);
        }
    }

    return cached_gif_count;
}

// Get the full path/filename of the GIF file with specified index
void getGIFFilenameByIndex(const char *directoryName, int index, char *pnBuffer) {
    // Make sure index is in range
    if ((index < 0) || (index >= cached_gif_count))
        return;

    // Copy the directory name into the pathname buffer			
    strcpy(pnBuffer, directoryName);
    
    //ESP32 SD Library includes the full path name in the filename, so no need to add the directory name
#if defined(ESP32)
    pnBuffer[0] = 0;
#else
    int len = strlen(pnBuffer);
    if (len == 0 || pnBuffer[len - 1] != '/') strcat(pnBuffer, "/");
#endif

    // Append the sorted filename to the pathname
    strcat(pnBuffer, cached_gif_filenames[index].c_str());
}

bool openGifFilenameByIndex(const char *directoryName, int index, char* name_buf) {
    char pathname[255];

    getGIFFilenameByIndex(directoryName, index, pathname);
    
    Serial.print("Pathname: ");
    Serial.println(pathname);

    if(my_sd_file)
        my_sd_file.close();

    // Attempt to open the file for reading
    my_sd_file = SD.open(pathname);
    if (!my_sd_file) {
        Serial.println("Error opening GIF file");
        return false;
    }

    strcpy(name_buf, my_sd_file.name());

    return true;
}


// Return a random animated gif path/filename from the specified directory
void chooseRandomGIFFilename(const char *directoryName, char *pnBuffer) {

    int index = random(numberOfFiles);
    getGIFFilenameByIndex(directoryName, index, pnBuffer);
}
