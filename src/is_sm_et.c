#include "pihm.h"

void IntcpSnowET (int t, double stepsize, pihm_struct pihm)
{
    int             i;
    const double    TSNOW = -3.0;
    const double    TRAIN = 1.0;
    const double    T0 = 0.0;

#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (i = 0; i < nelem; i++)
    {
        double      satn;
        double      betas;
        double      fr;
        double      alphar;
        double      etas;
        double      gammas;
        double      rs;
        double      pc;
        double      delta, gamma;
        double      radnet;
        double      sfctmp;
        double      wind;
        double      rh;
        double      vp;
        double      pres;
        double      lai;
        double      z0;
        double      ra;
        double      qvsat;
        double      qv;
        double      etp;
        double      isval = 0.0;
        double      frac_snow;
        double      snow_rate;
        double      melt_rate;
        double      intcp_max;
        double      meltf;
        elem_struct *elem;

        elem = &pihm->elem[i];

        /* Note the dependence on physical units */
        elem->ps.albedo = 0.5 * (elem->lc.albedomin + elem->lc.albedomax);
        radnet = elem->ef.soldn * (1.0 - elem->ps.albedo);
        sfctmp = elem->es.sfctmp - 273.15;
        wind = elem->ps.sfcspd;
        rh = elem->ps.rh / 100.0;

        vp = 611.2 * exp (17.67 * sfctmp / (sfctmp + 243.5)) * rh;
        pres =
            101.325 * 1.0e3 * pow ((293.0 - 0.0065 * elem->topo.zmax) / 293.0,
            5.26);
        qv = 0.622 * vp / pres;
        qvsat = 0.622 * (vp / rh) / pres;
        if (elem->attrib.lai_type > 0)
        {
            lai = elem->ps.proj_lai;
        }
        else
        {
            lai = MonthlyLAI (t, elem->attrib.lc_type);
        }

        meltf = MonthlyMF (t);

        /* Snow accumulation and snow melt calculation */
        frac_snow = (sfctmp < TSNOW) ? 1.0 :
            ((sfctmp > TRAIN) ? 0 : (TRAIN - sfctmp) / (TRAIN - TSNOW));
        snow_rate = frac_snow * elem->wf.prcp;

        elem->ws.sneqv += snow_rate * stepsize;

        melt_rate = (sfctmp > T0 ? (sfctmp - T0) * meltf : 0.0);

        if (elem->ws.sneqv > melt_rate * stepsize)
        {
            elem->ws.sneqv -= melt_rate * stepsize;
        }
        else
        {
            melt_rate = elem->ws.sneqv / stepsize;
            elem->ws.sneqv = 0.0;
        }

        /* ThroughFall and Evaporation from canopy */
        /*
         * EleIS, EleET[0] and ret are prorated for the whole element.
         * Logistics are simpler if assumed in volumetric form by
         * multiplication of Area on either side of equation */
        intcp_max = elem->lc.cmcfactr * lai * elem->lc.shdfac;

#ifdef _DEBUG_
        if (i == 0)
        {
            printf ("Max is %lf (%lf * %lf * %lf)\n", intcp_max,
                elem->lc.cmcfactr,
                lai,
                elem->lc.shdfac);
        }
#endif

        z0 = MonthlyRL (t, elem->attrib.lc_type);

        ra = log (elem->ps.zlvl_wind / z0) * log (10.0 *
            elem->ps.zlvl_wind / z0) / (wind * 0.16);

        gamma =
            4.0 * 0.7 * SIGMA * RD / CP * pow (sfctmp + 273.15,
            4) / (pres / ra) + 1.0;
        delta =
            LVH2O * LVH2O * 0.622 / RV / CP / pow (sfctmp + 273.15,
            2) * qvsat;

        etp =
            (radnet * delta + gamma * (1.2 * LVH2O * (qvsat -
                    qv) / ra)) / (1000.0 * LVH2O * (delta + gamma));

        if (elem->soil.depth - elem->ws.gw < elem->ps.rzd)
        {
            satn = 1.0;
        }
        else
        {
            satn =
                ((elem->ws.unsat / (elem->soil.depth - elem->ws.gw)) >
                1.0) ? 1.0 : ((elem->ws.unsat / (elem->soil.depth -
                        elem->ws.gw)) <
                0.0) ? 0.0 : 0.5 * (1.0 -
                cos (3.14 * (elem->ws.unsat / (elem->soil.depth -
                            elem->ws.gw))));
        }

        betas =
            (satn * elem->soil.porosity + elem->soil.smcmin -
            elem->soil.smcwlt) / (elem->soil.smcref - elem->soil.smcwlt);
        betas = (betas < 0.0001) ? 0.0001 : (betas > 1.0 ? 1.0 : betas);
		elem->wf.edir = etp;
        elem->wf.edir = (1.0 - elem->lc.shdfac) * pow (betas, 2) * etp * pihm->cal.edir;
        elem->wf.edir = elem->wf.edir < 0.0 ? 0.0 : elem->wf.edir;

        /* Note the dependence on physical units */
        if (lai > 0.0)
        {
            elem->wf.ec = elem->lc.shdfac *
                pow (((elem->ws.cmc < 0.0) ? 0.0 :
                ((elem->ws.cmc > intcp_max) ? intcp_max : elem->ws.cmc)) /
                    intcp_max, elem->lc.cfactr) * etp;
			elem->wf.ec = elem->wf.ec * pihm->cal.ec;
            elem->wf.ec = elem->wf.ec < 0.0 ? 0.0 : elem->wf.ec;

            fr = 1.1 * radnet / (elem->epc.rgl * lai);
            fr = (fr < 0.0) ? 0.0 : fr;
            alphar = (1.0 + fr) / (fr + (elem->epc.rsmin / elem->epc.rsmax));
            alphar = (alphar > 10000.0) ? 10000.0 : alphar;
            etas =
                1.0 - 0.0016 * (pow ((elem->epc.topt - 273.15 - sfctmp), 2));
            etas = etas < 0.0001 ? 0.0001 : etas;
            gammas = 1.0 / (1.0 + 0.00025 * (vp / rh - vp));
            gammas = (gammas < 0.01) ? 0.01 : gammas;
            rs = elem->epc.rsmin * alphar / (betas * lai * etas * gammas);
            rs = rs > elem->epc.rsmax ? elem->epc.rsmax : rs;

            pc = (1.0 + delta / gamma) / (1.0 + rs / ra + delta / gamma);

            elem->wf.ett = elem->lc.shdfac * pc *
                (1.0 - pow ((elem->ws.cmc < 0.0) ? 0.0 :
                ((elem->ws.cmc > intcp_max) ? intcp_max : elem->ws.cmc) /
                intcp_max, elem->lc.cfactr)) * etp;
			elem->wf.ett = elem->wf.ett * pihm->cal.ett;
            elem->wf.ett = elem->wf.ett < 0.0 ? 0.0 : elem->wf.ett;
            elem->wf.ett = ((elem->ws.gw < (elem->soil.depth - elem->ps.rzd))
                && elem->ws.unsat <= 0.0) ? 0.0 : elem->wf.ett;

            /* Drip function from Rutter and Morton, 1977, Journal of Applied
             * Ecology
             * D0 = 3.91E-5 m/min = 6.52E-7 m/s */
            elem->wf.drip = (elem->ws.cmc <= 0.0) ? 0.0 :
                6.52E-7 * intcp_max * exp (3.89 * elem->ws.cmc / intcp_max);
        }
        else
        {
            elem->wf.ett = 0.0;
            elem->wf.ec = 0.0;
            elem->wf.drip = 0.0;
        }

        if (elem->wf.drip < 0.0)
        {
            elem->wf.drip = 0.0;
        }
        if (elem->wf.drip * stepsize > elem->ws.cmc)
        {
            elem->wf.drip = elem->ws.cmc / stepsize;
        }

        isval = elem->ws.cmc +
            (1.0 - frac_snow) * elem->wf.prcp * elem->lc.shdfac * stepsize -
            elem->wf.ec * stepsize - elem->wf.drip * stepsize;

        if (isval > intcp_max)
        {
            elem->ws.cmc = intcp_max;
            elem->wf.drip += (isval - intcp_max) / stepsize;
        }
        else if (isval < 0.0)
        {
            elem->ws.cmc = 0.0;
            if (elem->wf.ec + elem->wf.drip > 0.0)
            {
                elem->wf.ec = elem->wf.ec / (elem->wf.ec + elem->wf.drip) *
                    (elem->ws.cmc +
                    (1.0 - frac_snow) * elem->wf.prcp * elem->lc.shdfac *
                    stepsize);
                elem->wf.drip =
                    elem->wf.drip / (elem->wf.ec + elem->wf.drip) *
                    (elem->ws.cmc +
                    (1.0 - frac_snow) * elem->wf.prcp * elem->lc.shdfac *
                    stepsize);
            }
        }
        else
        {
            elem->ws.cmc = isval;
        }

        elem->wf.pcpdrp =
            (1.0 - elem->lc.shdfac) * (1.0 - frac_snow) * elem->wf.prcp +
            elem->wf.drip + melt_rate;
    }
}
