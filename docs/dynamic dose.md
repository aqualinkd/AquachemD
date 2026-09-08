# ==========================================
# AQUACHEMD GLOBAL SETTINGS
# ==========================================
target_ph = 7.4
pump_flow_rate_ml_sec = 2.18
max_single_dose_ml = 150

# Choose your dosing method: "table" or "dynamic"
dosing_mode = "dynamic"

# ==========================================
# MODE 1: LOOKUP TABLE CONFIG
# (Only active if dosing_mode = "table")
# ==========================================
ph_dose_mapping = "8.0:150, 7.8:100, 7.7:75, 7.6:45, 7.4:15"

# ==========================================
# MODE 2: DYNAMIC PROPORTIONAL CONFIG
# (Only active if dosing_mode = "dynamic")
# ==========================================
pool_volume_gallons = 30000
acid_strength_percent = 15
dosing_aggressiveness = "Balanced"

---------------------------------------

#include <stdio.h>
#include <string.h>

// Structure to mimic the parsed configuration file variables
typedef struct {
    double target_ph;
    double pump_flow_rate_ml_sec;
    double max_single_dose_ml;
    int pool_volume_gallons;
    int acid_strength_percent;
    char dosing_aggressiveness[20]; // "Conservative", "Balanced", "Aggressive"
} AquachemConfig;

// Function to calculate pump runtime in seconds based on live pH
double calculate_dynamic_runtime(double current_ph, AquachemConfig config) {
    double min_pump_time_sec = 5.0; // Safety floor to protect pump gears/priming
    
    // 1. Calculate the pH variance (error)
    double ph_error = current_ph - config.target_ph;
    
    // If pH is at or below target, no dosing is required
    if (ph_error <= 0.0) {
        return 0.0;
    }
    
    // 2. Determine the aggressiveness multiplier (Dampening Factor)
    double multiplier = 0.20; // Default fallback to "Balanced"
    if (strcmp(config.dosing_aggressiveness, "Conservative") == 0) {
        multiplier = 0.10;
    } else if (strcmp(config.dosing_aggressiveness, "Balanced") == 0) {
        multiplier = 0.20;
    } else if (strcmp(config.dosing_aggressiveness, "Aggressive") == 0) {
        multiplier = 0.40;
    }

    // 3. Calculate True Chemical Demand (KP_FACTOR)
    // Baseline: 90ml of 30% acid drops a 30,000 gallon pool by 0.1 pH.
    // We scale this linearly by volume and inversely by acid concentration.
    double base_volume_for_point_one = 90.0 * ((double)config.pool_volume_gallons / 30000.0) * (30.0 / (double)config.acid_strength_percent);
        
    double true_chemical_kp = base_volume_for_point_one * 10.0; // Scale from 0.1 pH to 1.0 pH error

    // 4. Calculate target dose volume
    double target_volume_ml = ph_error * true_chemical_kp * multiplier;

    // 5. Apply Safety Clamp (Per-Dose Cap)
    if (target_volume_ml > config.max_single_dose_ml) {
        target_volume_ml = config.max_single_dose_ml;
    }

    // 6. Convert volume to pump runtime seconds
    double runtime_seconds = target_volume_ml / config.pump_flow_rate_ml_sec;

    // 7. Apply Minimum Runtime Floor Check
    if (runtime_seconds < min_pump_time_sec) {
        return 0.0; 
    }

    return runtime_seconds;
}

int main() {
    // Mocking an initialized configuration block for Shaun's pool setup
    AquachemConfig config = {
        .target_ph = 7.4,
        .pump_flow_rate_ml_sec = 2.18,
        .max_single_dose_ml = 150.0,
        .pool_volume_gallons = 30000,
        .acid_strength_percent = 15,
        .dosing_aggressiveness = "Balanced"
    };

    // Test Scenarios to see the logic in action
    double test_ph_readings[] = {7.42, 7.55, 7.80, 8.30};
    int num_tests = sizeof(test_ph_readings) / sizeof(test_ph_readings[0]);

    printf("AquachemD Dynamic Calculation Engine Test Run\n");
    printf("--------------------------------------------------\n");
    printf("Config: Target=%.1f, Vol=%d gal, Acid=%d%%, Mode=%s\n\n", 
           config.target_ph, config.pool_volume_gallons, 
           config.acid_strength_percent, config.dosing_aggressiveness);

    for (int i = 0; i < num_tests; i++) {
        double current_ph = test_ph_readings[i];
        double runtime = calculate_dynamic_runtime(current_ph, config);
        
        printf("Current pH Sensor Reading: %.2f\n", current_ph);
        if (runtime > 0.0) {
            double calculated_volume = runtime * config.pump_flow_rate_ml_sec;
            printf("  -> Calculated Dose: %.1f ml\n", calculated_volume);
            printf("  -> Command Relay ON for: %.2f seconds\n\n", runtime);
        } else {
            printf("  -> Action: No dose required (or below runtime safety threshold)\n\n");
        }
    }

    return 0;
}