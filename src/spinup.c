#include "pihm.h"

void Spinup(pihm_struct pihm, N_Vector CV_Y, void *cvode_mem,
    SUNLinearSolver *sun_ls)
{
    int             spinyears = 0;
    int             first_spin_cycle = 1;
    int             steady;
    int             metyears;
    ctrl_struct    *ctrl;

    ctrl = &pihm->ctrl;

    metyears = (ctrl->endtime - ctrl->starttime) / DAYINSEC / 365;

    do
    {
        pihm_printf(VL_BRIEF, "Spinup year: %6d\n", spinyears + 1);

#if defined(_BGC_)
        ResetSpinupStat(pihm->elem);
#endif

        for (ctrl->cstep = 0; ctrl->cstep < ctrl->nstep; ctrl->cstep++)
        {
            PIHM(0.0, pihm, cvode_mem, CV_Y);
        }

        /* Reset solver parameters */
        SetCVodeParam(pihm, cvode_mem, sun_ls, CV_Y);

        spinyears += metyears;

#if defined(_BGC_)
        steady = CheckSteadyState(first_spin_cycle,
            ctrl->endtime - ctrl->starttime, spinyears, pihm->siteinfo.area,
            pihm->elem);
#else
        steady = CheckSteadyState(first_spin_cycle, spinyears,
            pihm->siteinfo.area, pihm->elem);
#endif

        first_spin_cycle = 0;
    } while (spinyears < ctrl->maxspinyears &&
        ((!fixed_length && !steady) || fixed_length));
}

#if defined(_BGC_)
void ResetSpinupStat(elem_struct elem[])
{
    int             i;

#if defined(_LUMPEDBGC_)
    i = LUMPEDBGC;
#else
# if defined(_OPENMP)
#  pragma omp parallel for
# endif
    for (i = 0; i < nelem; i++)
#endif
    {
        elem[i].spinup.soilc = 0.0;
        elem[i].spinup.totalc = 0.0;
    }
}
#endif

#if defined(_BGC_)
int CheckSteadyState(int first_cycle, int totalt, int spinyears,
    double total_area, const elem_struct elem[])
#else
int CheckSteadyState(int first_cycle, int spinyears, double total_area,
    const elem_struct elem[])
#endif
{
    int             i;
    double          totalw = 0.0;
    static double   totalw_prev = 0.0;
    int             steady;
#if defined(_BGC_)
    double          t1 = 0.0;
    double          soilc = 0.0;
    double          totalc = 0.0;
    static double   soilc_prev = 0.0;
#endif
#if defined(_FBR_)
    double          gwgeol = 0.0;
    static double   gwgeol_prev = 0.0;
#endif

#if defined(_LUMPEDBGC_)
    i = LUMPEDBGC;
#else
    for (i = 0; i < nelem; i++)
#endif
    {
        totalw += (elem[i].ws.unsat + elem[i].ws.gw) * elem[i].soil.porosity *
            elem[i].topo.area / total_area;

#if defined(_FBR_)
        totalw += (elem[i].ws.fbr_unsat + elem[i].ws.fbr_gw) *
            elem[i].geol.porosity * elem[i].topo.area / total_area;

        gwgeol += elem[i].ws.fbr_gw * elem[i].topo.area / total_area;
#endif

#if defined(_BGC_)
        /* Convert soilc and totalc to average daily soilc */
        soilc += elem[i].spinup.soilc * elem[i].topo.area /
            (double)(totalt / DAYINSEC) / total_area;
        totalc += elem[i].spinup.totalc * elem[i].topo.area /
            (double)(totalt / DAYINSEC) / total_area;
        t1 = (soilc - soilc_prev) / (double)(totalt / DAYINSEC / 365);
#endif
    }

    if (!first_cycle)
    {
        /* Check if domain reaches steady state */
        steady = (fabs(totalw - totalw_prev) < SPINUP_W_TOLERANCE);
#if defined(_FBR_)
        steady = (steady && (fabs(gwgeol - gwgeol_prev) < SPINUP_W_TOLERANCE));
#endif
#if defined(_BGC_)
        steady = (steady && (fabs(t1) < SPINUP_C_TOLERANCE));
#endif

        pihm_printf(VL_BRIEF, "spinyears = %d ", spinyears);
        pihm_printf(VL_BRIEF, "totalw_prev = %lg totalw = %lg wdif = %lg\n",
            totalw_prev, totalw, totalw - totalw_prev);
#if defined(_FBR_)
        pihm_printf(VL_BRIEF, "fbrgw_prev = %lg fbrgw = %lg wdif = %lg\n",
            gwgeol_prev, gwgeol, gwgeol - gwgeol_prev);
#endif
#if defined(_BGC_)
        pihm_printf(VL_BRIEF, "soilc_prev = %lg soilc = %lg pdif = %lg\n",
            soilc_prev, soilc, t1);
#endif
    }
    else
    {
        steady = 0;
    }

    totalw_prev = totalw;
#if defined(_FBR_)
    gwgeol_prev = gwgeol;
#endif
#if defined(_BGC_)
    soilc_prev = soilc;
#endif

    if (steady)
    {
        pihm_printf(VL_BRIEF, "Reaches steady state after %d year.\n",
            spinyears);
    }

    return steady;
}
