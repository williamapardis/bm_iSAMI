# iSAMI Bristlemouth Interface Firmware
This includes the firmware for the Bristlemouth development kit to interface a Sofar Smart Spotter Mooring with Sunburst Sensor's iSAMI-pH instrument. The frimware functions as follows:

* Bristlemouth Mote triggers a measurement on the iSAMI through the RS232 connection at a specified interval.
* The iSAMI runs through its measurment sequence (~60s) and transmits its raw hex data through the RS232 connection.
* This 233 byte hex code is proccessed and a series of calculations lead to the determination of pH.
* Raw hex data is stored on the Spotter SD card through the Bristlemouth network
* pH and external temperature are stored on the Spotter SD and transmitted over the cellular modem.

We are demonstrating integration of Sunburst Sensor’s iSAMI-pH and Sofar Ocean’s Smart Spotter Buoy through the Bristlemouth Dev. kit. The Spotter and the iSAMI are well paired economically/electromechanically. However, the high cost of custom engineering integration still limits these cost effective solutions. With the adoption of Bristlemouth we mitigate costly integration. This strategic development has the opportunity to open up coastal pH monitoring to a wider range of users through the turn-key operability of Sofar’s ecosystem. This ecosystem as a gateway to the demonstrated success of Sunburst Sensor’s spectrophotometric measurement approach enables a higher density of accurate and low drift pH observations in the ocean.

![image](https://github.com/user-attachments/assets/d62f8649-4a80-4075-9dbc-c6d842b3e9b2)

## Dependencies
### Main Protocol
- BM Protocol v2.1.0
  - Repository: https://github.com/organization/bm_protocol

