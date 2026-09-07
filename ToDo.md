
<p align="center">
  <img src="web/aquachemd.png" width="120" alt="AquachemD logo">
</p>

<h1 align="center">AquachemD</h1>
<p align="center"><b>Open-source, automated pool water chemistry — pH, ORP, and dosing, done right.</b></p>
<hr>
<br>
<br>

- Decide on logic for GPIO_SWITCH and Global / Local lockouts.  ie include it or remove it.
- Global / Local lockout for H2O Pump (ie water fill).
- At present config is boolean for lockout, is xxxx_scope_global?  The above may need full input range of 3 ie (None, Local, Global) map to (ACD_SCOPE_ALLOW, ACD_SCOPE_LOCAL ACD_SCOPE_GLOBAL).
- implement use averages for dosing.
- Look at using SWG% to increase dose times. (Long shot, pain to implement)

