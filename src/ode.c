#include "pihm.h"

int ODE (realtype t, N_Vector CV_Y, N_Vector CV_Ydot, void *pihm_data)
{
    int             i;
    double         *y;
    double         *dy;
    double          dt;
    pihm_struct     pihm;


#ifdef _OPENMP
		y = NV_DATA_OMP(CV_Y);
		dy = NV_DATA_OMP(CV_Ydot);
#else
		y = NV_DATA_S(CV_Y);
		dy = NV_DATA_S(CV_Ydot);
#endif
    pihm = (pihm_struct)pihm_data;

    dt = (double)pihm->ctrl.stepsize;

    /*
     * Initialization of temporary state variables
     */
    for (i = 0; i < NSV; i++)
    {
        dy[i] = 0.0;
    }

#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (i = 0; i < nelem; i++)
    {
        elem_struct *elem;
        elem = &pihm->elem[i];

        elem->ws.surf = (y[SURF(i)] >= 0.0) ? y[SURF(i)] : 0.0;
        elem->ws.unsat = (y[UNSAT(i)] >= 0.0) ? y[UNSAT(i)] : 0.0;
        elem->ws.gw = (y[GW(i)] >= 0.0) ? y[GW(i)] : 0.0;

#ifdef _BGC_
        elem->ns.sminn = (y[SMINN(i)] >= 0.0) ? y[SMINN(i)] : 0.0;
#endif
    }

#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (i = 0; i < nriver; i++)
    {
        river_struct *riv;
        riv = &pihm->riv[i];

        riv->ws.stage = (y[RIVSTG (i)] >= 0.0) ? y[RIVSTG (i)] : 0.0;
        riv->ws.gw = (y[RIVGW (i)] >= 0.0) ? y[RIVGW (i)] : 0.0;

#ifdef _BGC_
        riv->ns.rivern = (y[RIVERN(i)] >= 0.0) ? y[RIVERN(i)] : 0.0;
#endif

        riv->wf.rivflow[UP_CHANL2CHANL] = 0.0;
        riv->wf.rivflow[UP_AQUIF2AQUIF] = 0.0;
    }

    /*
     * PIHM Hydrology
     */
    Hydrol (pihm);

#ifdef _BGC_
    NTransport (pihm);
#endif

    /*
     * Build RHS of ODEs
     */
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (i = 0; i < nelem; i++)
    {
        int         j;
        elem_struct *elem;

        elem = &pihm->elem[i];

        dy[SURF(i)] += elem->wf.pcpdrp - elem->wf.infil - elem->wf.edir_surf;
        dy[UNSAT(i)] += elem->wf.infil - elem->wf.rechg -
            elem->wf.edir_unsat - elem->wf.ett_unsat;
        dy[GW(i)] += elem->wf.rechg - elem->wf.edir_gw - elem->wf.ett_gw;

        for (j = 0; j < NUM_EDGE; j++)
        {
            dy[SURF(i)] -= elem->wf.ovlflow[j] / elem->topo.area;
            dy[GW(i)] -= elem->wf.subsurf[j] / elem->topo.area;
        }

        dy[UNSAT(i)] /= elem->soil.porosity;
        dy[GW(i)] /= elem->soil.porosity;

        if (isnan (dy[SURF(i)]))
        {
            PIHMprintf (VL_ERROR,
                "Error: NAN error for Element %d (surface water) at %lf\n",
                i + 1, t);
            PIHMexit (EXIT_FAILURE);
        }
        if (isnan (dy[UNSAT(i)]))
        {
            PIHMprintf (VL_ERROR,
                "Error: NAN error for Element %d (unsat water) at %lf\n",
                i + 1, t);
            PIHMexit (EXIT_FAILURE);
        }
        if (isnan (dy[GW(i)]))
        {
            PIHMprintf (VL_ERROR,
                "Error: NAN error for Element %d (groundwater) at %lf\n",
                i + 1, t);
            PIHMexit (EXIT_FAILURE);
        }

#ifdef _BGC_
        dy[SMINN(i)] +=
            (elem->nf.ndep_to_sminn + elem->nf.nfix_to_sminn) / DAYINSEC +
            elem->nsol.snksrc;

        for (j = 0; j < NUM_EDGE; j++)
        {
            dy[SMINN(i)] -= elem->nsol.flux[j] / elem->topo.area;
        }

        if (isnan (dy[SMINN(i)]))
        {
            PIHMprintf (VL_ERROR,
                "Error: NAN error for Element %d (soil mineral N) at %lf\n",
                i + 1, t);
            PIHMexit (EXIT_FAILURE);
        }
#endif
    }

#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (i = 0; i < nriver; i++)
    {
        int         j;
        river_struct *riv;

        riv = &(pihm->riv[i]);

        for (j = 0; j <= 6; j++)
        {
            /* Note the limitation due to
             * d(v)/dt=a*dy/dt+y*da/dt
             * for cs other than rectangle */
            dy[RIVSTG (i)] -= riv->wf.rivflow[j] / riv->topo.area;
        }

        dy[RIVGW (i)] += 0.0 -
            riv->wf.rivflow[LEFT_AQUIF2AQUIF] -
            riv->wf.rivflow[RIGHT_AQUIF2AQUIF] -
            riv->wf.rivflow[DOWN_AQUIF2AQUIF] -
            riv->wf.rivflow[UP_AQUIF2AQUIF] + riv->wf.rivflow[CHANL_LKG];

        dy[RIVGW (i)] /= riv->matl.porosity * riv->topo.area;

        if (isnan (dy[RIVSTG (i)]))
        {
            PIHMprintf (VL_ERROR,
                "Error: NAN error for River Segment %d (stage) at %lf\n",
                i + 1, t);
            PIHMexit (EXIT_FAILURE);
        }
        if (isnan (dy[RIVGW (i)]))
        {
            PIHMprintf (VL_ERROR,
                "Error: NAN error for River Segment %d (groundwater) at"
                "%lf\n", i + 1, t);
            PIHMexit (EXIT_FAILURE);
        }

#ifdef _BGC_
        dy[RIVERN (i)] -= riv->nsol.flux[UP] + riv->nsol.flux[DOWN] +
            riv->nsol.flux[LEFT] + riv->nsol.flux[RIGHT];

        dy[RIVERN (i)] /= riv->topo.area;

        if (isnan (dy[RIVERN(i)]))
        {
            PIHMprintf (VL_ERROR,
                "Error: NAN error for River Segment %d (mineral N) at %lf\n",
                i + 1, t);
            PIHMexit (EXIT_FAILURE);
        }
#endif
    }

    return (0);
}

void SetCVodeParam (pihm_struct pihm, void *cvode_mem, N_Vector CV_Y)
{
    int             flag;

    flag = CVodeInit (cvode_mem, ODE, (realtype)0.0, CV_Y);
    flag = CVodeSStolerances (cvode_mem,(realtype) pihm->ctrl.reltol,
		(realtype) pihm->ctrl.abstol);
    flag = CVodeSetUserData (cvode_mem, pihm);
    flag = CVodeSetInitStep (cvode_mem, (realtype) pihm->ctrl.initstep);
    flag = CVodeSetStabLimDet (cvode_mem, TRUE);
    flag = CVodeSetMaxStep (cvode_mem, (realtype) pihm->ctrl.maxstep);
    flag = CVSpgmr (cvode_mem, PREC_NONE, 0);
}

void SolveCVode (int starttime, int *t, int nextptr, int stepsize, double cputime,
		void *cvode_mem, N_Vector CV_Y, char *simulation, char *outputdir)

{
    realtype        solvert;
    realtype        cvode_val;
    realtype        tout = (realtype)(nextptr - starttime);
    pihm_t_struct   pihm_time;
    int             flag;

	solvert = (realtype) (*t);
    flag = CVodeSetMaxNumSteps (cvode_mem, 0);
    flag = CVodeSetStopTime (cvode_mem, tout);
    flag = CVode (cvode_mem, (realtype) nextptr, CV_Y, &solvert, CV_NORMAL);
    flag = CVodeGetCurrentTime (cvode_mem, &cvode_val);

    *t = (int)round (solvert + starttime);

    pihm_time = PIHMTime (*t);
    if (debug_mode)
    {
        PIHMprintf (VL_NORMAL, " Step = %s (%d)\n", pihm_time.str, *t);
    }
    else if (spinup_mode)
    {
        if (pihm_time.t % DAYINSEC == 0)
        {
            PIHMprintf (VL_NORMAL, " Step = %s\n", pihm_time.str);
        }
    }
    else if (pihm_time.t % 3600 == 0)
    {
        PIHMprintf (VL_NORMAL, " Step = %s %f\n", pihm_time.str, cputime);
    }
}
