#include "pihm.h"

void DailyBgc(pihm_struct pihm, int t)
{

    int             i;
    double          co2lvl;
    double          dayl, prev_dayl;
    double          ndep, nfix;
    spa_data        spa, prev_spa;

    /* Get co2 and ndep */
    if (spinup_mode)    /* Spinup mode */
    {
        co2lvl = pihm->co2.co2ppm;
        ndep = pihm->ndepctrl.ndep / 365.0;
        nfix = pihm->ndepctrl.nfix / 365.0;
    }
    else                /* Model mode */
    {
        /* Atmospheric CO2 handling */
        if (!(pihm->co2.varco2))
        {
            /* Constant CO2, constant Ndep */
            co2lvl = pihm->co2.co2ppm;
            ndep = pihm->ndepctrl.ndep / 365.0;
            nfix = pihm->ndepctrl.nfix / 365.0;
        }
        else
        {
            co2lvl = GetCO2(pihm->forc.co2[0], t);
        }

        /* Ndep handling */
        if (!(pihm->ndepctrl.varndep))
        {
            /* Constant Ndep */
            ndep = pihm->ndepctrl.ndep / 365.0;
            nfix = pihm->ndepctrl.nfix / 365.0;
        }
        else
        {
            ndep = GetNdep(pihm->forc.ndep[0], t);
            ndep = ndep / 365.0;
            nfix = pihm->ndepctrl.nfix / 365.0;
        }
    }

    /* Calculate daylengths */
    SunPos(t, pihm->siteinfo.latitude, pihm->siteinfo.longitude,
        pihm->siteinfo.elevation, pihm->siteinfo.tavg, &spa);
    SunPos(t - DAYINSEC, pihm->siteinfo.latitude, pihm->siteinfo.longitude,
        pihm->siteinfo.elevation, pihm->siteinfo.tavg, &prev_spa);

    dayl = (spa.sunset - spa.sunrise) * 3600.0;
    dayl = (dayl < 0.0) ? dayl + 24.0 * 3600.0 : dayl;

    prev_dayl = (prev_spa.sunset - prev_spa.sunrise) * 3600.0;
    prev_dayl = (prev_dayl < 0.0) ? prev_dayl + 24.0 * 3600.0 : prev_dayl;

#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (i = 0; i < nelem; i++)
    {
        int             k;
        daily_struct   *daily;
        epconst_struct *epc;
        epvar_struct   *epv;
        soil_struct    *soil;
        wstate_struct  *ws;
        estate_struct  *es;
        eflux_struct   *ef;
        pstate_struct  *ps;
        cstate_struct  *cs;
        cflux_struct   *cf;
        nstate_struct  *ns;
        nflux_struct   *nf;
        ntemp_struct   *nt;
        solute_struct  *nsol;
        psn_struct     *psn_sun, *psn_shade;
        summary_struct *summary;
        double          vwc;
        int             annual_alloc;

        daily = &pihm->elem[i].daily;
        epc = &pihm->elem[i].epc;
        epv = &pihm->elem[i].epv;
        soil = &pihm->elem[i].soil;
        ws = &pihm->elem[i].ws;
        es = &pihm->elem[i].es;
        ef = &pihm->elem[i].ef;
        ps = &pihm->elem[i].ps;
        cs = &pihm->elem[i].cs;
        cf = &pihm->elem[i].cf;
        ns = &pihm->elem[i].ns;
        nf = &pihm->elem[i].nf;
        nt = &pihm->elem[i].nt;
        nsol = &pihm->elem[i].nsol;
        psn_sun = &pihm->elem[i].psn_sun;
        psn_shade = &pihm->elem[i].psn_shade;
        summary = &pihm->elem[i].summary;

        /* Determine daylengths */
        epv->dayl = dayl;
        epv->prev_dayl = prev_dayl;

        /* Determine CO2 level */
        ps->co2 = co2lvl;

        PrecisionControl(cs, ns);

        /* Zero all the flux variables */
        MakeZeroFluxStruct(cf, nf);

        /* Phenology fluxes */
        Phenology(epc, epv, cs, cf, ns, nf, daily);

        /* Test for the annual allocation day */
        if (epv->offset_flag == 1 && epv->offset_counter == 1)
        {
            annual_alloc = 1;
        }
        else
        {
            annual_alloc = 0;
        }

        /* Calculate leaf area index, sun and shade fractions, and specific
         * leaf area for sun and shade canopy fractions, then calculate
         * canopy radiation interception and transmission */
        RadTrans(cs, ef, ps, epc, epv, daily);

        /* Soil water potential */
        vwc = daily->avg_sh2o[0] * ps->sldpth[0];
        if (ps->nsoil > 1)
        {
            for (k = 1; k < ps->nsoil; k++)
            {
                vwc += daily->avg_sh2o[k] * ps->sldpth[k];
            }
        }
        vwc /= soil->depth;

        SoilPsi(soil, vwc, &epv->psi);

        /* Maintenance respiration */
        MaintResp(epc, epv, cs, cf, ns, daily);

        /* Begin canopy bio-physical process simulation */
        if (cs->leafc && epv->dayl)
        {
            /* Conductance */
            CanopyCond(epc, epv, ef, ps, soil, daily);
        }

        /* Do photosynthesis only when it is part of the current growth season,
         * as defined by the remdays_curgrowth flag. This keeps the occurrence
         * of new growth consistent with the treatment of litterfall and
         * allocation */
        if (cs->leafc && !epv->dormant_flag && epv->dayl)
        {
            TotalPhotosynthesis(epc, epv, ps, cf, psn_sun, psn_shade, daily);
        }
        else
        {
            epv->assim_sun = epv->assim_shade = 0.0;
        }

        nf->ndep_to_sminn = ndep;
        nf->nfix_to_sminn = nfix;

        /* Daily litter and soil decomp and nitrogen fluxes */
        Decomp(daily->avg_stc[0] - TFREEZ, epc, epv, cs, cf, ns, nf, nt);

        /* Allocation gets called whether or not this is a current growth day,
         * because the competition between decomp immobilization fluxes and
         * plant growth N demand is resolved here. On days with no growth, no
         * allocation occurs, but immobilization fluxes are updated
         * normally */
        DailyAllocation(cf, cs, nf, ns, epc, epv, nt);

        /* Growth respiration */
        GrowthResp(epc, cf);

        /* Update of carbon state variables */
        DailyCarbonStateUpdate(cf, cs, annual_alloc, epc->woody,
            epc->evergreen);

        /* Update of nitrogen state variables */
        DailyNitrogenStateUpdate(nf, ns, nsol, annual_alloc, epc->woody,
            epc->evergreen);

        /* Calculate mortality fluxes and update state variables */
        /* This is done last, with a special state update procedure, to ensure
         * that pools don't go negative due to mortality fluxes conflicting with
         * other proportional fluxes */
        Mortality(epc, cs, cf, ns, nf);

        /* Test for carbon balance */
        CheckCarbonBalance(cs, &epv->old_c_balance);

#if OBSOLETE
        /* Nitrogen balance is checked outside DailyBgc function because a bgc
         * cycle is not finished until N transport is caluculated by CVODE */
        CheckNitrogenBalance (ns, &epv->old_n_balance);
#endif

        /* Calculate carbon summary variables */
        CSummary(cf, cs, summary);

        if (spinup_mode)
        {
            pihm->elem[i].spinup.soilc += summary->soilc;
            pihm->elem[i].spinup.totalc += summary->totalc;
        }
    }
}
