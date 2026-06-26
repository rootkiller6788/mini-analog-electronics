/**
 * @file    test_oscillator.c
 * @brief   Test suite for mini-oscillator-circuit
 */
#include "oscillator_core.h"
#include "oscillator_rc.h"
#include "oscillator_lc.h"
#include "oscillator_crystal.h"
#include "oscillator_relaxation.h"
#include "oscillator_analysis.h"
#include "oscillator_pll.h"
#include "oscillator_vco.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static int passed = 0, run = 0;
#define T(nm) do{run++;printf("  TEST %2d: "nm,run);fflush(stdout)
#define E() passed++;printf("PASS\n");}while(0)

int main(void){
printf("\n=== mini-oscillator-circuit Test Suite ===\n\n");
T("params init");oscillator_params_t p=oscillator_params_init(OSC_TYPE_RC_PHASE_SHIFT,1000,5);assert(p.amplitude_vpp>0);E();
T("crystal params");oscillator_params_t p2=oscillator_params_init(OSC_TYPE_PIERCE_CRYSTAL,16e6,3.3);assert(p2.thd_percent<0.1);E();
T("Q factor");quality_factor_t q=quality_factor_compute(1000,1e-6,1e-9,1);assert(q.q_value>20&&q.q_value<50);E();
T("Q from BW");assert(fabs(quality_factor_from_bandwidth(10e6,100e3)-100)<1);E();
T("barkhausen ok");barkhausen_criterion_t b=barkhausen_evaluate(1.5,0,1000);assert(b.magnitude_satisfied==1);assert(b.phase_satisfied==1);E();
T("barkhausen fail");barkhausen_criterion_t b2=barkhausen_evaluate(0.5,0,1000);assert(b2.magnitude_satisfied==0);E();
T("barkhausen startup");assert(barkhausen_startup_check(12,OSC_TYPE_WIEN_BRIDGE)==1);assert(barkhausen_startup_check(3,OSC_TYPE_WIEN_BRIDGE)==0);E();
T("bark find freq");loop_gain_sweep_t sw;sw.num_points=5;double f[]={800,900,1000,1100,1200};double ph[]={-10,-5,0,5,10};sw.freq_hz=f;sw.phase_deg=ph;sw.magnitude=NULL;sw.magnitude_db=NULL;assert(fabs(barkhausen_find_osc_freq(&sw)-1000)<1);E();
T("rc phase shift");rc_phase_shift_osc_t o1=rc_phase_shift_design(1000,0,5);assert(o1.freq_osc_hz>900&&o1.freq_osc_hz<1100);E();
T("wien bridge");wien_bridge_osc_t o2=wien_bridge_design(1000,0);assert(o2.freq_osc_hz>800&&o2.freq_osc_hz<1200);E();
T("colpitts");colpitts_osc_t o3=colpitts_design(10e6,0.3,0);assert(o3.freq_osc_hz>5e6&&o3.freq_osc_hz<20e6);E();
T("hartley");hartley_osc_t o4=hartley_design(1e6,0.2);assert(o4.freq_osc_hz>500e3&&o4.freq_osc_hz<2e6);E();
T("pierce");pierce_osc_t o5=pierce_design(16e6,0,3.3);assert(o5.freq_osc_hz>15.9e6&&o5.freq_osc_hz<16.1e6);E();
T("555");relaxation_555_t o6=relaxation_555_design(1000,60);assert(o6.freq_hz>800&&o6.freq_hz<1200);E();
T("schmitt");relaxation_schmitt_t o7=relaxation_schmitt_design(10000,5,0);assert(o7.freq_hz>5000&&o7.freq_hz<20000);E();
T("van der pol");van_der_pol_params_t pp=van_der_pol_init(1,1);van_der_pol_sim_t s=van_der_pol_simulate(&pp,0.1,0.1,20,0.01);assert(s.num_points>100);van_der_pol_sim_free(&s);E();
T("leeson PN");leeson_phase_noise_t pn=leeson_phase_noise_compute(100e6,10e3,50,10e-3,2,0,290);assert(pn.phase_noise_dbc_hz<-50);E();
T("hajimiri PN");hajimiri_phase_noise_t hp=hajimiri_phase_noise_compute(0.5,1e-12,1e-18,1e6,0,0);assert(hp.phase_noise_dbc_hz<-80);E();
T("lc tank");lc_tank_t t=lc_tank_analyze(1e-6,100e-12,1,0);assert(t.resonant_freq_hz>14e6&&t.resonant_freq_hz<18e6);E();
T("rc bark verify");rc_phase_shift_osc_t oa=rc_phase_shift_design(1000,0,5);assert(rc_phase_shift_verify_barkhausen(&oa).freq_hz>0);E();
T("wien bark verify");wien_bridge_osc_t ob=wien_bridge_design(1000,0);assert(wien_bridge_verify_barkhausen(&ob).freq_hz>0);E();
T("crystal model");crystal_model_t x=crystal_model_create(10e6,0,0);assert(x.fs_hz>9.9e6&&x.fs_hz<10.1e6);E();
T("crystal pull");crystal_model_t x2=crystal_model_create(10e6,0,0);double pc=crystal_pullability_ppm(&x2,18e-12);assert(pc>0&&pc<1000);E();
T("crystal Z");crystal_model_t x3=crystal_model_create(10e6,0,0);double re,im;crystal_impedance(&x3,x3.fs_hz,&re,&im);assert(re>0&&re<1000);E();
T("clapp");clapp_osc_t oc=clapp_design(50e6,50);assert(oc.freq_osc_hz>40e6&&oc.freq_osc_hz<60e6);E();
T("THD compute");double a[]={10,1,0.5,0.2,0.1};double thd=thd_compute(a,5);assert(thd>10&&thd<20);E();
T("THD pure");double a2[]={1,0,0,0};assert(thd_compute(a2,4)<1e-6);E();
T("lc pulling");double df=lc_oscillator_pulling(100e6,50,2);assert(df>0&&df<2e6);E();
T("startup time");double ts=oscillator_startup_time_estimate(50,10e6,1,3.3);assert(ts>0&&ts<1);E();
T("xtal startup");pierce_osc_t op=pierce_design(10e6,0,3.3);assert(crystal_startup_time(&op)>0);E();
T("CP PLL");charge_pump_pll_t pll=charge_pump_pll_design(10e6,1e9,500e3,50,100e6,100e-6);assert(pll.system.n_divider==100);E();
T("PLL stable");charge_pump_pll_t pll2=charge_pump_pll_design(10e6,1e9,500e3,50,100e6,100e-6);assert(pll_stability_check(&pll2.system)==1);E();
T("PLL lock");charge_pump_pll_t pll3=charge_pump_pll_design(10e6,1e9,500e3,50,100e6,100e-6);double tl=pll_lock_time_estimate(&pll3.system,10e6);assert(tl>0&&tl<0.1);E();
T("LC VCO");lc_vco_t v=lc_vco_design(2e9,3e9,3.3,10);assert(v.freq_center_hz>1.0e9&&v.freq_center_hz<5.0e9);assert(v.inductance_h>0.0);assert(v.power_mw>0.0);E();
T("varactor");varactor_t vr=varactor_init(2e-12,2,3.3);double c0=varactor_capacitance(&vr,0);double c3=varactor_capacitance(&vr,3.3);assert(c0>c3);E();
T("VCO FOM");lc_vco_t v2=lc_vco_design(2e9,3e9,3.3,10);assert(vco_figure_of_merit(&v2)<0);E();
T("ring VCO");ring_vco_t rv=ring_vco_design(1e9,500e6,1.8);assert(rv.num_stages==3);E();
T("GPS L1");lc_vco_t v3=lc_vco_design(1.57e9,1.58e9,3.3,15);assert(v3.freq_center_hz>1.0e9&&v3.freq_center_hz<5.0e9);E();
T("null safety");oscillator_params_t pn2=oscillator_params_init(OSC_TYPE_WIEN_BRIDGE,1000,5);assert(oscillator_estimate_thd(&pn2)>0);E();
T("zero freq");rc_phase_shift_osc_t oz=rc_phase_shift_design(0,0,5);assert(oz.freq_osc_hz==0);E();
T("neg input");quality_factor_t qn=quality_factor_compute(-1000,1e-6,1e-9,1);assert(qn.q_value==0);E();
T("allan var");double m[]={1e-6,1.1e-6,0.9e-6,1.05e-6,0.95e-6};assert(allan_variance_compute(m,5,1).allan_deviation>0);E();
T("jitter");double fo[]={1e3,1e4,1e5,1e6,1e7};double pnj[]={-60,-80,-100,-120,-140};jitter_metrics_t j=jitter_from_phase_noise(100e6,pnj,fo,5);assert(j.rms_phase_jitter_ps>0);E();
T("tolerance");tolerance_analysis_t tol={5,10,0,0,0,0,0};assert(oscillator_tolerance_analyze(1000,&tol).freq_dev_ppm>0);E();
T("opamp relax");relaxation_opamp_t or2=relaxation_opamp_design(1000,5,12);assert(or2.freq_hz>500&&or2.freq_hz<2000);E();
T("twin-T");twin_t_osc_t ott=twin_t_design(1000,10);assert(ott.freq_osc_hz>800&&ott.freq_osc_hz<1200);E();
T("armstrong");armstrong_osc_t oar=armstrong_design(1e6,0.1);assert(oar.freq_osc_hz>500e3&&oar.freq_osc_hz<2e6);E();
T("x-coupled LC");cross_coupled_lc_osc_t ox=cross_coupled_lc_design(5e9,10,10);assert(ox.freq_osc_hz>1.0e9&&ox.freq_osc_hz<10.0e9);assert(ox.startup_margin>0);E();
T("VCO tuning");lc_vco_t vt=lc_vco_design(2e9,3e9,3.3,10);double flo=lc_vco_frequency(&vt,0.3);double fhi=lc_vco_frequency(&vt,3.3);assert(fhi>=flo);double kv=lc_vco_kvco(&vt,1.65);assert(kv>=0.0);E();
T("colpitts bark");colpitts_osc_t ocb=colpitts_design(10e6,0.3,0);assert(colpitts_verify_barkhausen(&ocb).freq_hz>0);E();
T("pierce bark");pierce_osc_t opb=pierce_design(16e6,0,3.3);assert(pierce_verify_barkhausen(&opb).freq_hz>0);E();
T("pn->jitter");double foj[]={1e3,1e4,1e5,1e6,1e7};double pnj2[]={-60,-80,-100,-120,-140};double j2=phase_noise_to_rms_jitter(100e6,1e3,1e7,pnj2,foj,5);assert(j2>0&&j2<1e-6);E();
T("freq stab");oscillator_params_t pfs=oscillator_params_init(OSC_TYPE_PIERCE_CRYSTAL,10e6,3.3);frequency_stability_t sfs;memset(&sfs,0,sizeof(sfs));frequency_stability_analyze(&pfs,&sfs);assert(sfs.temp_coefficient_ppm_per_c>0);E();
T("rc ladder");rc_phase_shift_osc_t orl=rc_phase_shift_design(1000,0,5);double mg,ph2;rc_ladder_transfer(orl.r_ohms,orl.c_farads,orl.freq_osc_hz,&mg,&ph2);assert(mg>0&&mg<1);E();
T("wien tf");wien_bridge_osc_t owt=wien_bridge_design(1000,0);double mg2,ph3;wien_bridge_transfer(owt.r_series_ohms,owt.c_series_farads,owt.freq_osc_hz,&mg2,&ph3);assert(fabs(mg2-0.333)<0.01);E();
T("drive level");pierce_osc_t odl=pierce_design(16e6,0,1.8);assert(odl.drive_level_uw>0.0);assert(crystal_drive_level_check(&odl,500)==1);E();
T("555 validate");relaxation_555_t ov5=relaxation_555_design(1000,60);assert(relaxation_555_validate(&ov5,5)==1);E();
printf("\n=== Results: %d/%d tests passed ===\n\n",passed,run);
return (passed==run)?0:1;
}
