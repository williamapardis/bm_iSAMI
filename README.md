# iSAMI Bristlemouth Interface Firmware
This includes the firmware for the Bristlemouth development kit to interface a Sofar Smart Spotter Mooring with Sunburst Sensor's iSAMI-pH instrument. The frimware functions as follows:

* Bristlemouth Mote triggers a measurement on the iSAMI through the RS232 connection at a specified interval.
* The iSAMI runs through its measurment sequence (~60s) and transmits its raw hex data through the RS232 connection.
* This 233 byte hex code is proccessed and a series of calculations lead to the determination of pH.
* Raw hex data is stored on the Spotter SD card through the Bristlemouth network
* pH and external temperature are stored on the Spotter SD and transmitted over the cellular modem.

## Dependencies
### Main Protocol
- BM Protocol v2.1.0
  - Repository: https://github.com/organization/bm_protocol

## Wiring of the iSAMI-pH to the Bristlemouth Dev Kit
Please note that I have secured the development board to the oposite endcap it comes pre installed on with the Dev. Kit. This lets us utilize the 2P Molex Microfit interconnect for assembly of the pressure vessel, rather than adding our own.
| Pin to Pin Discription | Example Image of termination |
|---------|---------|
| <img src="https://github.com/user-attachments/assets/6d64efa2-74bb-4ad2-95e1-c6ffe2b3db1b" width="500">| <img src="https://github.com/user-attachments/assets/b9764d9f-1ab8-4112-ad9a-9e87ab9b2d72" width="300">|

# The Project
We're trying to put powerful chemistry in the hands of a wider demographic and lower the barrier to entry through standardized integration of well established oceanographic assets. 

This is the integration and deomonstraition of Sunburst Sensor’s iSAMI-pH and Sofar Ocean’s Smart Spotter Buoy through the Bristlemouth Dev. kit. The Spotter and the iSAMI are well paired economically/electromechanically. However, the high cost of custom engineering integration still limits these cost effective solutions. With the adoption of Bristlemouth we mitigate costly integration. This strategic development has the opportunity to open up coastal pH monitoring to a wider range of users through the turn-key operability of Sofar’s ecosystem. This ecosystem as a gateway to the demonstrated success of Sunburst Sensor’s spectrophotometric measurement approach enables a higher density of accurate and low drift pH observations in the ocean.

![image](https://github.com/user-attachments/assets/a3537980-f597-458e-b787-2da7cac826e3)

## How does the iSAMI-pH work?
Most people are familiar with litmus paper—strips containing pH-sensitive dye that change color when exposed to different solutions. The iSAMI-pH is essentially an automated version of litmus paper, but instead of visual observation, it mixes liquid dye with seawater and analyzes the color change using precise embedded optical electronics. We would call this a spectrophotometric pH method: spectro - (color), photo - (light), metric - (measuring). Here is the dye (m-Cresol Purple) in action:

<p align="center">
  <img src="https://github.com/user-attachments/assets/d657caad-3810-43e3-91d2-ba19436170aa" width="300">
</p>

The advantage of spectrophotometric pH is that, rather than taking a calibrated approach like electrodes, it directly measures hydrogen ions in solution. measure of hydrogen ions in solution. This calibration free method consequently has very low drift, allowing these sensors to dwell for years while maintaining their accuracy.
Thanks to the exacting work of analytical chemists, this dye’s physical properties have been scrutinized in the seawater matrix, making it highly accurate. It is considered the gold standard for quantitative oceanographic pH measurement.

Thanks to Sunburst Sensor’s engineering and attention to detail, we have instruments like the iSAMI-pH, which realistically bring this measurement into the real world ocean environment. Here is a nice [overview of Sunburst](https://youtu.be/aXZPkW4L6uA?si=S3U1Ic4FXqChgCz4) as they were working to compete in the Wendy Schmidt Ocean-X prize. An important note [Sunburst Sensor took first place](https://www.pmel.noaa.gov/news-story/wendy-schmidt-ocean-health-xprize-winners-announced#:~:text=Winning%20first%20place%20in%20both,each%20team%20took%20home%20%24250%2C000.) in both categories: Accuracy and affordability. 


## Why pH?
If you were to take a single drop of seawater from the central pacific and compare it to one 30 years ago there would be ~42 billion more hydrogen ions in that single drop. Increased carbon dioxide in our atmosphere reacts with our ocean producing hydrogen ions in solution and perturbing our ocean's fundamental chemistry. How do we conceptualize such large changes in hydrogen ion concentration? The practical answer is pH. pH compresses the wild extremes of hydrogen ion concentration into a tidy, 14-step logarithmic scale.

$$
pH = log_{10}([H^+]),\ \ \ \ [H^+] \equiv \text{Hydtrogen Ion Concentraition}
$$
