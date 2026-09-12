#include "charpy_calc.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static bool nearly_equal(float actual, float expected)
{
    return fabsf(actual - expected) < 0.0005f;
}

int main(void)
{
    float energy = charpy_calc_energy(3.950f, 0.400f, 150.0f, -120.0f);
    assert(nearly_equal(energy, 5.6714f));

    float reference_strength = 0.0f;
    assert(charpy_calc_impact_strength(energy, 10.0f, 10.0f, 2.0f,
                                       &reference_strength));
    assert(nearly_equal(reference_strength, 7.0892f));

    float area_cm2 = 0.0f;
    assert(charpy_calc_net_area_cm2(10.0f, 10.0f, 2.0f, &area_cm2));
    assert(nearly_equal(area_cm2, 0.8f));

    float strength = 0.0f;
    assert(charpy_calc_impact_strength(8.0f, 10.0f, 10.0f, 2.0f, &strength));
    assert(nearly_equal(strength, 10.0f));

    float narrow_strength = 0.0f;
    float deep_notch_strength = 0.0f;
    assert(charpy_calc_impact_strength(8.0f, 5.0f, 10.0f, 2.0f, &narrow_strength));
    assert(charpy_calc_impact_strength(8.0f, 10.0f, 10.0f, 4.0f, &deep_notch_strength));
    assert(nearly_equal(narrow_strength, 20.0f));
    assert(nearly_equal(deep_notch_strength, 13.3333f));

    assert(!charpy_calc_net_area_cm2(0.0f, 10.0f, 2.0f, &area_cm2));
    assert(nearly_equal(area_cm2, 0.0f));
    assert(!charpy_calc_net_area_cm2(10.0f, 10.0f, -1.0f, &area_cm2));
    assert(!charpy_calc_net_area_cm2(10.0f, 10.0f, 10.0f, &area_cm2));
    assert(!charpy_calc_net_area_cm2(10.0f, 10.0f, 11.0f, &area_cm2));
    assert(!charpy_calc_impact_strength(NAN, 10.0f, 10.0f, 2.0f, &strength));
    assert(nearly_equal(strength, 0.0f));

    puts("charpy_calc tests passed");
    return 0;
}