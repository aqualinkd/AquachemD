<p align="center">
  <img src="../web/aquachemd.png" width="120" alt="AquachemD logo">
</p>
<h1 align="center">AquachemD</h1>
<p align="center"><b>Open-source, automated pool water chemistry — pH, ORP, and dosing, done right.</b></p>
<hr>
<br>
<br>

---

## Information on different Dosing Strategies

I have played around with how the major manufacturers do dosing and it quite frankly sucks over a basic lookup table. 

They will take a target pH and pool size, then over a set number of doses try to bring the pH down to the target using a VERY generic formula.  Usually dose every 15 mins and try to bring to target within 1 to 2 hours. (depending on how far off pH is from target). These would potentially work well if it learned from history and modified the formula. [More information is here](#manufacturersdosingstrategies)

Those dosing strategies are not currently deployed with AquachemD, but if someone wants it, please ask and I can add a configuration option to use a formula instead of table.

### Basic lookup table works exceptionally well.

What I have found works a LOT better is a simply lookup table that you modify for your specific pool and how often you want to dose. 

The amount of acid needed for a given pH reduction actually depends on the water's alkalinity, and as of writing this, no known controller manufacturer adjusts its acid dosing based on measured alkalinity — meaning controllers rely purely on their own fixed programming, with no way to account for what's actually happening in that specific pool's water chemistry. The stated consequence is that chemical controllers, tend to systematically overdose acid.
That's why generic time/volume-based formulas underperform a tuned lookup table.

Below are the exact values I use.

| pH Reading | Seconds to Dose |
| --- | --- |
|8.2|120|
|8.1|80|
|8.0|50|
|7.8|30|
|7.75|20|
|7.7|10|
|7.65|5|
|7.6|2|
|7.5|0|

Other Pool information.
| | |
| --- | --- |
| pH target | 7.7 |
| Pool size | 30,000 Gal |
| Pool Material | Plaster |
| Pool type | Salt Water |
| Dose schedule | every 30 minutes between 6am and 11pm |



How I originally got to those numbers.  I was using on average 1gal of 30% Acid per week to keep pool in check. So I took that number and divided it into how many doses I had scheduled for the week, plus accounted for using 15% Acid since that's what I use in the Acid tank.

~230 Doses a week needs to dose 2 Gal of Acid, (my pump is 2.18 ml |/s)

**Convert total volume to milliliters (mL):**

$$2 \text{ US gallons} \times 3,785.41 \text{ ml |/gallon} = 7,570.82 \text{ ml |}$$


**Calculate volume per dose:**

$$\frac{7,570.82 \text{ ml |}}{230 \text{ doses}} = 32.92 \text{ ml | per dose}$$


**Calculate run time per dose:**

$$\frac{32.92 \text{ ml |}}{2.18 \text{ ml |/s}} = 15.10 \text{ seconds}$$

So that was my starting point and I configured my dosing table with pH 7.7 being a 15 seconds dose and went up and down from that.
Below was my first table, a simply starting point.
| pH Reading | Seconds to Dose |
| --- | --- |
|8|60|
|7.8|30|
|7.7|15|
|7.6|0|

Over the next few days I monitored pH and Dose in seconds, and quickly realized I didn't need nearly as much as I had scheduled, and kept modifying the table.  It took me about 1 week to land on the table posted at the top of this article, and as you can see I'm now using 1/2 as much Acid as I used too when manually adding Acid.<br>
I was using 1 gal of 32% Acid a week, now using just under 1 gal of 15% Acid a week. So 1/2 as much.

Below is my last weeks dose stats as of writing this. Last line `Acid doser TOTAL: 3679.84ml (0.97gal) over 7 Days`

Anytime you see a line Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml, that's due to an interlock being on, so unsafe to dose (usually pool cleaner being on).

BTW, this is pulled directly from AquachemD, so you don't need to manually record/monitor this. 
 
| Date / Time | Run Time | pH reading | ml Dosed|
| --- | --- | --- | --- |
|Sun Sep 13 06:20:06 2026| Run time: 5s | Sensor reading 7.69 | Dosed: 10.90 ml |
|Sun Sep 13 06:50:06 2026| Run time: 5s | Sensor reading 7.69 | Dosed: 10.90 ml |
|Sun Sep 13 07:20:06 2026| Run time: 5s | Sensor reading 7.69 | Dosed: 10.90 ml |
|Sun Sep 13 07:50:06 2026| Run time: 5s | Sensor reading 7.69 | Dosed: 10.90 ml |
|Sun Sep 13 08:20:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Sun Sep 13 08:50:03 2026| Run time: 2s | Sensor reading 7.61 | Dosed: 4.36 ml |
|Sun Sep 13 09:20:03 2026| Run time: 2s | Sensor reading 7.60 | Dosed: 4.36 ml |
|Sun Sep 13 09:50:03 2026| Run time: 2s | Sensor reading 7.61 | Dosed: 4.36 ml |
|Sun Sep 13 10:20:03 2026| Run time: 2s | Sensor reading 7.65 | Dosed: 4.36 ml |
|Sun Sep 13 10:50:07 2026| Run time: 5s | Sensor reading 7.66 | Dosed: 10.90 ml |
|Sun Sep 13 11:20:06 2026| Run time: 5s | Sensor reading 7.70 | Dosed: 10.90 ml |
|Sun Sep 13 11:50:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Sun Sep 13 12:20:11 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Sun Sep 13 12:50:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Sun Sep 13 13:20:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Sun Sep 13 13:50:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Sun Sep 13 14:20:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Sun Sep 13 14:50:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Sun Sep 13 15:20:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Sun Sep 13 15:50:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Sun Sep 13 16:20:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Sun Sep 13 16:50:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Sun Sep 13 17:20:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Sun Sep 13 17:50:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Sun Sep 13 18:20:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Sun Sep 13 18:50:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Sun Sep 13 19:20:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Sun Sep 13 19:50:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Sun Sep 13 20:20:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Sun Sep 13 20:50:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Sun Sep 13 21:20:12 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Sun Sep 13 21:50:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Sun Sep 13 22:20:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Sun Sep 13 22:50:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Mon Sep 14 06:20:06 2026| Run time: 5s | Sensor reading 7.69 | Dosed: 10.90 ml |
|Mon Sep 14 06:50:11 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Mon Sep 14 07:20:12 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Mon Sep 14 07:50:11 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Mon Sep 14 08:20:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Mon Sep 14 08:50:03 2026| Run time: 2s | Sensor reading 7.64 | Dosed: 4.36 ml |
|Mon Sep 14 09:20:03 2026| Run time: 2s | Sensor reading 7.64 | Dosed: 4.36 ml |
|Mon Sep 14 09:50:06 2026| Run time: 5s | Sensor reading 7.66 | Dosed: 10.90 ml |
|Mon Sep 14 10:20:06 2026| Run time: 5s | Sensor reading 7.69 | Dosed: 10.90 ml |
|Mon Sep 14 10:50:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Mon Sep 14 11:20:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Mon Sep 14 11:50:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Mon Sep 14 12:20:11 2026| Run time: 10s | Sensor reading 7.73 | Dosed: 21.80 ml |
|Mon Sep 14 12:50:12 2026| Run time: 10s | Sensor reading 7.73 | Dosed: 21.80 ml |
|Mon Sep 14 13:20:11 2026| Run time: 10s | Sensor reading 7.73 | Dosed: 21.80 ml |
|Mon Sep 14 13:50:11 2026| Run time: 10s | Sensor reading 7.73 | Dosed: 21.80 ml |
|Mon Sep 14 14:20:11 2026| Run time: 10s | Sensor reading 7.73 | Dosed: 21.80 ml |
|Mon Sep 14 14:50:11 2026| Run time: 10s | Sensor reading 7.73 | Dosed: 21.80 ml |
|Mon Sep 14 15:20:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Mon Sep 14 15:50:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Mon Sep 14 16:20:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Mon Sep 14 16:50:11 2026| Run time: 10s | Sensor reading 7.73 | Dosed: 21.80 ml |
|Mon Sep 14 17:20:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Mon Sep 14 17:50:11 2026| Run time: 10s | Sensor reading 7.73 | Dosed: 21.80 ml |
|Mon Sep 14 18:20:11 2026| Run time: 10s | Sensor reading 7.73 | Dosed: 21.80 ml |
|Mon Sep 14 18:50:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Mon Sep 14 19:20:11 2026| Run time: 10s | Sensor reading 7.73 | Dosed: 21.80 ml |
|Mon Sep 14 19:50:11 2026| Run time: 10s | Sensor reading 7.73 | Dosed: 21.80 ml |
|Mon Sep 14 20:20:11 2026| Run time: 10s | Sensor reading 7.73 | Dosed: 21.80 ml |
|Mon Sep 14 20:50:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Mon Sep 14 21:20:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Mon Sep 14 21:50:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Mon Sep 14 22:20:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Mon Sep 14 22:50:11 2026| Run time: 10s | Sensor reading 7.73 | Dosed: 21.80 ml |
|Tue Sep 15 06:20:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Tue Sep 15 06:50:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Tue Sep 15 07:20:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Tue Sep 15 07:50:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Tue Sep 15 08:20:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Tue Sep 15 08:50:06 2026| Run time: 5s | Sensor reading 7.65 | Dosed: 10.90 ml |
|Tue Sep 15 09:20:03 2026| Run time: 2s | Sensor reading 7.63 | Dosed: 4.36 ml |
|Tue Sep 15 09:50:06 2026| Run time: 5s | Sensor reading 7.68 | Dosed: 10.90 ml |
|Tue Sep 15 10:20:06 2026| Run time: 5s | Sensor reading 7.68 | Dosed: 10.90 ml |
|Tue Sep 15 10:50:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Tue Sep 15 11:20:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Tue Sep 15 11:50:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Tue Sep 15 12:20:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Tue Sep 15 12:50:11 2026| Run time: 10s | Sensor reading 7.73 | Dosed: 21.80 ml |
|Tue Sep 15 13:20:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Tue Sep 15 13:50:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Tue Sep 15 14:20:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Tue Sep 15 14:50:12 2026| Run time: 10s | Sensor reading 7.73 | Dosed: 21.80 ml |
|Tue Sep 15 15:20:11 2026| Run time: 10s | Sensor reading 7.73 | Dosed: 21.80 ml |
|Tue Sep 15 15:50:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Tue Sep 15 16:20:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Tue Sep 15 16:50:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Tue Sep 15 17:20:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Tue Sep 15 17:50:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Tue Sep 15 18:20:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Tue Sep 15 18:50:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Tue Sep 15 19:20:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Tue Sep 15 19:50:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Tue Sep 15 20:20:12 2026| Run time: 10s | Sensor reading 7.73 | Dosed: 21.80 ml |
|Tue Sep 15 20:50:11 2026| Run time: 10s | Sensor reading 7.73 | Dosed: 21.80 ml |
|Tue Sep 15 21:20:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Tue Sep 15 21:50:11 2026| Run time: 10s | Sensor reading 7.73 | Dosed: 21.80 ml |
|Tue Sep 15 22:20:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Tue Sep 15 22:50:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Wed Sep 16 06:20:12 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Wed Sep 16 06:50:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Wed Sep 16 07:20:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Wed Sep 16 07:50:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Wed Sep 16 08:20:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Wed Sep 16 08:50:06 2026| Run time: 5s | Sensor reading 7.67 | Dosed: 10.90 ml |
|Wed Sep 16 09:20:03 2026| Run time: 2s | Sensor reading 7.64 | Dosed: 4.36 ml |
|Wed Sep 16 09:50:06 2026| Run time: 5s | Sensor reading 7.67 | Dosed: 10.90 ml |
|Wed Sep 16 10:20:06 2026| Run time: 5s | Sensor reading 7.70 | Dosed: 10.90 ml |
|Wed Sep 16 10:50:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Wed Sep 16 11:20:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Wed Sep 16 11:50:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Wed Sep 16 12:20:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Wed Sep 16 12:50:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Wed Sep 16 13:20:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Wed Sep 16 13:50:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Wed Sep 16 14:20:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Wed Sep 16 14:50:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Wed Sep 16 15:20:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Wed Sep 16 15:50:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Wed Sep 16 16:20:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Wed Sep 16 16:50:12 2026| Run time: 10s | Sensor reading 7.75 | Dosed: 21.80 ml |
|Wed Sep 16 17:20:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Wed Sep 16 17:50:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Wed Sep 16 18:20:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Wed Sep 16 18:50:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Wed Sep 16 19:20:11 2026| Run time: 10s | Sensor reading 7.75 | Dosed: 21.80 ml |
|Wed Sep 16 19:50:21 2026| Run time: 20s | Sensor reading 7.75 | Dosed: 43.60 ml |
|Wed Sep 16 20:20:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Wed Sep 16 20:50:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Wed Sep 16 21:20:11 2026| Run time: 10s | Sensor reading 7.74 | Dosed: 21.80 ml |
|Wed Sep 16 21:50:11 2026| Run time: 10s | Sensor reading 7.73 | Dosed: 21.80 ml |
|Wed Sep 16 22:20:12 2026| Run time: 10s | Sensor reading 7.73 | Dosed: 21.80 ml |
|Wed Sep 16 22:50:11 2026| Run time: 10s | Sensor reading 7.73 | Dosed: 21.80 ml |
|Thu Sep 17 06:20:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Thu Sep 17 06:50:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Thu Sep 17 07:20:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Thu Sep 17 07:50:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Thu Sep 17 08:20:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Thu Sep 17 08:50:06 2026| Run time: 5s | Sensor reading 7.69 | Dosed: 10.90 ml |
|Thu Sep 17 09:20:06 2026| Run time: 5s | Sensor reading 7.67 | Dosed: 10.90 ml |
|Thu Sep 17 09:50:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Thu Sep 17 10:20:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Thu Sep 17 10:50:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Thu Sep 17 11:20:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Thu Sep 17 11:50:11 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Thu Sep 17 12:20:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Thu Sep 17 12:50:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Thu Sep 17 13:20:12 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Thu Sep 17 13:50:11 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Thu Sep 17 14:20:11 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Thu Sep 17 14:50:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Thu Sep 17 15:20:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Thu Sep 17 15:50:11 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Thu Sep 17 16:20:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Thu Sep 17 16:50:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Thu Sep 17 17:20:11 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Thu Sep 17 17:50:11 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Thu Sep 17 18:20:06 2026| Run time: 5s | Sensor reading 7.70 | Dosed: 10.90 ml |
|Thu Sep 17 18:50:12 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Thu Sep 17 19:20:06 2026| Run time: 5s | Sensor reading 7.69 | Dosed: 10.90 ml |
|Thu Sep 17 19:50:06 2026| Run time: 5s | Sensor reading 7.70 | Dosed: 10.90 ml |
|Thu Sep 17 20:20:06 2026| Run time: 5s | Sensor reading 7.70 | Dosed: 10.90 ml |
|Thu Sep 17 20:50:06 2026| Run time: 5s | Sensor reading 7.69 | Dosed: 10.90 ml |
|Thu Sep 17 21:20:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Thu Sep 17 21:50:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Thu Sep 17 22:20:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Thu Sep 17 22:50:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Fri Sep 18 06:20:06 2026| Run time: 5s | Sensor reading 7.69 | Dosed: 10.90 ml |
|Fri Sep 18 06:50:06 2026| Run time: 5s | Sensor reading 7.69 | Dosed: 10.90 ml |
|Fri Sep 18 07:20:06 2026| Run time: 5s | Sensor reading 7.69 | Dosed: 10.90 ml |
|Fri Sep 18 07:50:06 2026| Run time: 5s | Sensor reading 7.68 | Dosed: 10.90 ml |
|Fri Sep 18 08:20:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Fri Sep 18 08:50:06 2026| Run time: 5s | Sensor reading 7.65 | Dosed: 10.90 ml |
|Fri Sep 18 09:20:06 2026| Run time: 5s | Sensor reading 7.65 | Dosed: 10.90 ml |
|Fri Sep 18 09:50:06 2026| Run time: 5s | Sensor reading 7.66 | Dosed: 10.90 ml |
|Fri Sep 18 10:20:06 2026| Run time: 5s | Sensor reading 7.66 | Dosed: 10.90 ml |
|Fri Sep 18 10:50:06 2026| Run time: 5s | Sensor reading 7.69 | Dosed: 10.90 ml |
|Fri Sep 18 11:17:15 2026| Run time: 5s | Sensor reading 7.71 | Dosed: 10.90 ml |
|Fri Sep 18 11:20:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Fri Sep 18 11:50:06 2026| Run time: 5s | Sensor reading 7.70 | Dosed: 10.90 ml |
|Fri Sep 18 12:20:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Fri Sep 18 12:50:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Fri Sep 18 13:20:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Fri Sep 18 13:50:06 2026| Run time: 5s | Sensor reading 7.70 | Dosed: 10.90 ml |
|Fri Sep 18 14:20:11 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Fri Sep 18 14:50:11 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Fri Sep 18 15:20:12 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Fri Sep 18 15:50:11 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Fri Sep 18 16:20:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Fri Sep 18 16:50:11 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Fri Sep 18 17:20:11 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Fri Sep 18 17:50:11 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Fri Sep 18 18:20:11 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Fri Sep 18 18:50:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Fri Sep 18 19:20:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Fri Sep 18 19:50:11 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Fri Sep 18 20:20:11 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Fri Sep 18 20:50:12 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Fri Sep 18 21:20:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Fri Sep 18 21:50:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Fri Sep 18 22:20:11 2026| Run time: 10s | Sensor reading 7.70 | Dosed: 21.80 ml |
|Fri Sep 18 22:50:06 2026| Run time: 5s | Sensor reading 7.70 | Dosed: 10.90 ml |
|Sat Sep 19 06:20:06 2026| Run time: 5s | Sensor reading 7.69 | Dosed: 10.90 ml |
|Sat Sep 19 06:50:07 2026| Run time: 5s | Sensor reading 7.69 | Dosed: 10.90 ml |
|Sat Sep 19 07:20:06 2026| Run time: 5s | Sensor reading 7.69 | Dosed: 10.90 ml |
|Sat Sep 19 07:50:06 2026| Run time: 5s | Sensor reading 7.69 | Dosed: 10.90 ml |
|Sat Sep 19 08:20:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Sat Sep 19 08:50:06 2026| Run time: 5s | Sensor reading 7.66 | Dosed: 10.90 ml |
|Sat Sep 19 09:20:03 2026| Run time: 2s | Sensor reading 7.64 | Dosed: 4.36 ml |
|Sat Sep 19 09:50:06 2026| Run time: 5s | Sensor reading 7.66 | Dosed: 10.90 ml |
|Sat Sep 19 10:20:01 2026| Run time: 0s | Sensor reading 0.00 | Dosed: 0.00 ml |
|Sat Sep 19 10:50:06 2026| Run time: 5s | Sensor reading 7.69 | Dosed: 10.90 ml |
|Sat Sep 19 11:20:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Sat Sep 19 11:50:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |
|Sat Sep 19 12:20:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Sat Sep 19 12:50:11 2026| Run time: 10s | Sensor reading 7.72 | Dosed: 21.80 ml |
|Sat Sep 19 13:20:11 2026| Run time: 10s | Sensor reading 7.71 | Dosed: 21.80 ml |


---------------------------------------------

PMP_1 Acid doser TOTAL: 3679.84ml (0.97gal) over 1688s<br>
Stats Window Tracking Covers: Last 6 days 7 hours<br>

---------------------------------------------

<br><br><br><br><br><br>

<a id="manufacturersdosingstrategies"></a>

# Information on manufacturers dosing strategies.

From Pentair's actual IntelliChem installation and troubleshooting manuals

IntelliChem's Auto Setup wizard calculates dose amounts, mix time, and limits automatically based on pool volume and filter run time, with default dose times set by that wizard based on pool volume and filter run time rather than computed fresh from each reading. That's the key structural difference from AquachemD's lookup table: IntelliChem's "Dose by Time" mode computes one fixed dose duration up front from static pool parameters and reuses it every time it doses, rather than varying the dose per-event based on how far off the current reading is.

IntelliChem does have a proportional-response feature, but it's coarser than AquachemD's table. IntelliChem can adjust pH feed times based on how close the current reading is to the set point, to help prevent overshooting; the default "Low" sensitivity gives a full dose at 0.5 pH above setpoint, "High" gives a full dose at 0.2 pH above setpoint, and "Off" at just 0.005 pH above setpoint. So it's not literally a flat, single formula — there's a threshold-scaling concept in there — but it's a 3-position preset, not a fully custom curve like AquachemD's 10-point table. AquachemD's table is effectively a much higher-resolution version of the same idea.

IntelliChem enforces a default pH dose limit based on 2 ppm of pool size, up to a 5 oz maximum, tracked and reset every 24 hours, with dosing suspended once the limit is hit until the next period or a manual clear. AquachemD has the same cap, but completely user configurable both in ml dose and time period to cap.

IPS. A patent describing an IPS M920W-based system lays out the actual trigger logic: when ORP drops below setpoint, the controller sends a signal for a preset feed duration to route water through the chlorine feeder, then stops once that duration elapses — a fixed dose length per triggering event, not a value computed fresh from how far off the reading is. The pH side works the same way — once pH is detected as high, the controller signals the acid pump to feed muriatic acid until pH returns to setpoint. This is a different strategy than IntelliChem's fixed-time dosing — this is "run until you hit the target or time out," the same run-to-setpoint mode Pentair also offers as an alternative to Dose-by-Time. A related patent for a broader automatic chemical monitor confirms the same pattern generally across the category: the acid feed runs until either a satisfactory pH is sensed or a pump time limit is exceeded, with the actual feed duration chosen the same way as the chlorine system's

Pool Technologie's Dosipool Pro markets "Smart pH" specifically as predictive regulation based on history, alongside dosing proportional to pool volume, for what they describe as a more stable pH result. Their separate Pro Dosing ORP product describes both pH and chlorine injections as proportional to the gap between setpoint and the probe's actual measurement, aimed at reducing oscillation around the target compared to fixed-duration dosing.  This may be the exception to the <i>"most manufacturers dosing strategies suck"</i> statement at the top of this page.