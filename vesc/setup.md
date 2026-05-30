# VESC Setup

❕Note: These settings are only to be used for an **Xiaomi Elite**.

Setup the ESC using the [VESC tool](https://vesc-project.com/vesc_tool).

## Auto Wizard Setup (Setup Motors FOC)

1. Chose Generic
2. Chose DD hub motor
3. Battery:
   - *Type:* **Li-ion**
   - *Cells:* **10**
   - *Capacity:* **10 Ah** <br>
  Advanced:
      - *Regen:* **-8 A** (0.8 C)
      - *Current max:*<br>
				**19 A** (safe value, roughly 700 W at 36V)<br>
				**25 A** (overdrive value, roughly 900 W at 36V, **requires BMS bypass as OG limits to ~21**)<br>
				The battery theoretical max is around 30 A but introduces extreme degradation.
1. Setup:
   - *Direct drive:* **checked**
   - *Wheel diameter:* **254 mm (10 inch)**
   - *Motor poles:* **30**
   - *Temperature sensor:* **NTC 10k at 25C**
   - *Beta value for motor thermistor:* **3380K**

## Set/verify settings after the wizard

### Motor settings -> General

##### Current
- *Motor current max:* **37.5 A** <br>
Speed is proportional to voltage, so at lower speed the motor needs more current to keep the same wattage, over 40 A would probably damage the motor long term and mostly generate more heat.
- *Motor current max brake:* **-35 A**
- *Absolute maximum current:* **50 A** (For sudden spikes, if current goes over this threshold the VESC triggers an immediate emergency shutdown)
- *Battery current max:*<br>
	**19 A** (safe value, roughly 700 W at 36V)<br>
	**25 A** (overdrive value, roughly 900 W at 36V, **requires BMS bypass as OG limits to ~21**)<br>
	The battery theoretical max is around 30 A but introduces extreme degradation.
- *Battery current max regen:* **-8 A** (0.8 C)

#### Voltage
- *Battery voltage cutoff start:* **34 V** (when the battery voltage drops below this threshold, the VESC starts to limit the power to prevent over-discharging the battery)
- *Battery voltage cutoff end:* (when the battery voltage drops below this threshold, the VESC will completely cut off the power to protect the battery)<br>
	**32 V** (less battery degradation)<br>
	**30 V** (value under this will cause permanent damage)
- *Battery voltage regen cutoff start:* **42.5 V** (start to limit regen when the battery voltage start reaching this threshold)
- *Battery voltage regen cutoff end:* **43 V** (cut off regen when the battery voltage start reaching this threshold, prevents overcharging the battery)

#### BMS
- *BMS type:* **None** (VESC do not communicate with the BMS)

#### Advanced
- Duty cycle current limit start: **90%** (smooths out the cutoff of power at top speed to prevent jerking)


## Enabling Field Weakening

Field weakening works by injecting a extra current in the d-axis to weaken the magnetic field of the motor, allowing it to spin faster than its natural speed. This current do not give any extra torque, it will only generate extra heat so it drastically reduce efficiency.

**⚠️ DANGER ⚠️**


**Enabling field weakening poses several risks to both the motor and the rider:**
- *Rider safety:* If the VESC in someway stop working while the scooter is at high speed using the field weakening, the VESC will stop supplying current to keep the field weakening active and will cause the scooter to break hard as the field is no longer weakened. Or in worst case a short in the motor phases, that would causes an imimediate wheel lockup. Having the motor in the back wheel neglect the risk of catastrophic falls somewhat. Using this setting on a front wheel would be extremely dangerous and is not recommended.
- *Motor Damage:* The extra heat may damage the motor, this should only be used with motors that have a temperature sensor so one can set a temperature limit to prevent overheating.

Enable these settings for field weakening to work properly:

### Motor settings -> FOC
#### Field weakening
- *Field weakening current max:* **10 A** (Good starting point, can be increased if the motor can handle the extra heat)
- *Field weakening duty start:* **85%** (when the field weakening starts to take effect)
- *Field weakening ramp time:* **500 ms** (ramp time for the field weakening to reach its maximum effect)

### Motor settings -> General
#### RPM
- *Max ERPM:* **12 600** (results in 40.2 km/h)
- *ERPM limit start:* **95%** (when the motor reaches 95% of the max ERPM, the VESC starts to limit the power to prevent a hard cutoff of power at top speed)