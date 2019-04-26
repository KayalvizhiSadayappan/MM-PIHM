/*******************************************************************************
* RT-Flux-PIHM is a finite volume based, reactive transport module that operates
* on top of the hydrological land surface processes described by Flux-PIHM.
* RT-Flux-PIHM tracks the transportation and reaction in a given watershed. It
* uses operator splitting technique to couple transport and reaction.
*****************************************************************************/
#include "pihm.h"

void InitChem(const char cdbs_filen[], const calib_struct *cal,
    forc_struct *forc, chemtbl_struct chemtbl[], kintbl_struct kintbl[],
    rttbl_struct *rttbl)
{
    int             i;
    int             chem_ind;
    FILE           *fp;

    fp = fopen(cdbs_filen, "r");
    CheckFile(fp, cdbs_filen);

    /*
     * Look up database to find required parameters and dependencies for
     * chemical species
     */
    Lookup(fp, cal, chemtbl, kintbl, rttbl);

    /*
     * Apply calibration
     */
    rttbl->pumps[0].Injection_rate *= cal->gwinflux;
    rttbl->pumps[0].flow_rate *= cal->gwinflux;

    chem_ind = FindChem("'DOC'", chemtbl, rttbl->NumStc);
    if (chem_ind >= 0)
    {
        rttbl->prcp_conc[chem_ind] *= cal->prcpconc;

        if (forc->PrpFlg == 2)
        {
            for (i = 0; i < forc->TSD_prepconc.length; i++)
            {
                forc->TSD_prepconc.data[i][chem_ind] *= cal->prcpconc;
            }
        }
    }

    fclose(fp);
}

void InitRTVar(chemtbl_struct chemtbl[], rttbl_struct *rttbl,
    elem_struct elem[], river_struct river[], N_Vector CV_Y)
{
    int             i;

    /*
     * Initializing element concentrations
     */
    PIHMprintf(VL_VERBOSE, "\n Initializing concentrations... \n");

    for (i = 0; i < nelem; i++)
    {
        double          vol_gw;
        double          vol_unsat;

        vol_gw = MAX(GWStrg(elem[i].soil.depth, elem[i].soil.smcmax,
            elem[i].soil.smcmin, elem[i].ws.gw), DEPTHR) *
            elem[i].topo.area;
        vol_unsat = MAX(UnsatWaterStrg(elem[i].soil.depth, elem[i].soil.smcmax,
            elem[i].soil.smcmin, elem[i].ws.gw, elem[i].ws.unsat), DEPTHR) *
            elem[i].topo.area;

        InitChemS(chemtbl, rttbl, elem[i].restart_input.tconc_unsat,
            elem[i].restart_input.ssa_unsat, elem[i].soil.smcmax, vol_unsat,
            &elem[i].chms_unsat);

        InitChemS(chemtbl, rttbl, elem[i].restart_input.tconc_gw,
            elem[i].restart_input.ssa_gw, elem[i].soil.smcmax, vol_gw,
            &elem[i].chms_gw);

#if defined(_FBR_)
        vol_gw = MAX(GWStrg(elem[i].geol.depth, elem[i].geol.smcmax,
            elem[i].geol.smcmin, elem[i].ws.fbr_gw), DEPTHR) *
            elem[i].topo.area;
        vol_unsat = MAX(UnsatWaterStrg(elem[i].geol.depth, elem[i].geol.smcmax,
            elem[i].geol.smcmin, elem[i].ws.fbr_gw, elem[i].ws.fbr_unsat),
            DEPTHR) * elem[i].topo.area;

        InitChemS(chemtbl, rttbl, elem[i].restart_input.tconc_fbrunsat,
            elem[i].restart_input.ssa_fbrunsat, elem[i].geol.smcmax, vol_unsat,
            &elem[i].chms_fbrunsat);

        InitChemS(chemtbl, rttbl, elem[i].restart_input.tconc_fbrgw,
            elem[i].restart_input.ssa_fbrgw, elem[i].geol.smcmax, vol_gw,
            &elem[i].chms_fbrgw);
#endif
    }

    /*
     * Initialize river concentrations
     */
    for (i = 0; i < nriver; i++)
    {
        double          vol_rivbed;
        double          vol_stream;
        int             k;

        vol_rivbed = MAX(RivBedStrg(&river[i].matl, &river[i].ws), DEPTHR) *
            river[i].topo.area;
        vol_stream = river[i].topo.area * MAX(river[i].ws.stage, DEPTHR);

        for (k = 0; k < rttbl->NumStc; k++)
        {
            if (chemtbl[k].itype == AQUEOUS)
            {
                river[i].chms_stream.t_conc[k] =
                    0.5 * elem[river[i].leftele - 1].chms_gw.t_conc[k] +
                    0.5 * elem[river[i].rightele - 1].chms_gw.t_conc[k];
                river[i].chms_stream.p_actv[k] = river[i].chms_stream.t_conc[k];
                river[i].chms_stream.p_conc[k] = river[i].chms_stream.t_conc[k];
                river[i].chms_stream.t_mole[k] =
                    river[i].chms_stream.t_conc[k] * vol_stream;

                river[i].chms_rivbed.t_conc[k] =
                    0.5 * elem[river[i].leftele - 1].chms_gw.t_conc[k] +
                    0.5 * elem[river[i].rightele - 1].chms_gw.t_conc[k];
                river[i].chms_rivbed.p_actv[k] = river[i].chms_rivbed.t_conc[k];
                river[i].chms_rivbed.p_conc[k] = river[i].chms_rivbed.t_conc[k];
                river[i].chms_rivbed.t_mole[k] =
                    river[i].chms_rivbed.t_conc[k] * vol_rivbed;
            }
            else
            {
                river[i].chms_stream.t_conc[k] = ZERO_CONC;
                river[i].chms_stream.p_conc[k] = ZERO_CONC;
                river[i].chms_stream.p_actv[k] = ZERO_CONC;
                river[i].chms_stream.t_mole[k] = 0.0;

                river[i].chms_rivbed.t_conc[k] = ZERO_CONC;
                river[i].chms_rivbed.p_conc[k] = ZERO_CONC;
                river[i].chms_rivbed.p_actv[k] = ZERO_CONC;
                river[i].chms_rivbed.t_mole[k] = 0.0;
            }
        }

        for (k = 0; k < rttbl->NumSsc; k++)
        {
            river[i].chms_stream.s_conc[k] = ZERO_CONC;
            river[i].chms_rivbed.s_conc[k] = ZERO_CONC;
        }
    }

#if defined(_OPENMP)
# pragma omp parallel for
#endif
    for (i = 0; i < nelem; i++)
    {
        int             k;

        for (k = 0; k < NumSpc; k++)
        {
            NV_Ith(CV_Y, UNSAT_MOLE(i, k)) = elem[i].chms_unsat.t_mole[k];
            NV_Ith(CV_Y, GW_MOLE(i, k)) = elem[i].chms_gw.t_mole[k];

            elem[i].chmf.react_unsat[k] = 0.0;
            elem[i].chmf.react_gw[k] = 0.0;

#if defined(_FBR_)
            NV_Ith(CV_Y, FBRUNSAT_MOLE(i, k)) = elem[i].chms_fbrunsat.t_mole[k];
            NV_Ith(CV_Y, FBRGW_MOLE(i, k)) = elem[i].chms_fbrgw.t_mole[k];

            elem[i].chmf.react_fbrunsat[k] = 0.0;
            elem[i].chmf.react_fbrgw[k] = 0.0;
#endif
        }
    }

#if defined(_OPENMP)
# pragma omp parallel for
#endif
    for (i = 0; i < nriver; i++)
    {
        int             k;

        for (k = 0; k < NumSpc; k++)
        {
            NV_Ith(CV_Y, STREAM_MOLE(i, k)) = river[i].chms_stream.t_mole[k];
            NV_Ith(CV_Y, RIVBED_MOLE(i, k)) = river[i].chms_rivbed.t_mole[k];
        }
    }
}

void InitChemS(const chemtbl_struct chemtbl[], const rttbl_struct *rttbl,
    double t_conc[], double ssa[], double smcmax, double vol,
    chmstate_struct *chms)
{
    int             k;

    for (k = 0; k < rttbl->NumStc; k++)
    {
        if (strcmp(chemtbl[k].ChemName, "'H+'") == 0)
        {
            chms->t_conc[k] = t_conc[k];
            chms->p_actv[k] = chms->t_conc[k];
            chms->p_conc[k] = chms->t_conc[k];
            chms->ssa[k] = ssa[k];
        }
        else if (chemtbl[k].itype == MINERAL)
        {
            chms->t_conc[k] = t_conc[k];
            /* Update the concentration of mineral using molar volume */
            chms->t_conc[k] *= (rttbl->RelMin == 0) ?
                /* Absolute mineral volume fraction */
                1000.0 / chemtbl[k].MolarVolume / smcmax :
                /* Relative mineral volume fraction */
                (1.0 - smcmax) * 1000.0 / chemtbl[k].MolarVolume / smcmax;
            chms->p_actv[k] = 1.0;
            chms->p_conc[k] = chms->t_conc[k];
            chms->ssa[k] = ssa[k];
        }
        else if ((chemtbl[k].itype == CATION_ECHG) ||
            (chemtbl[k].itype == ADSORPTION))
        {
            chms->t_conc[k] = t_conc[k];
            chms->p_actv[k] = chms->t_conc[k] * 0.5;
            /* Change unit of CEC (eq g-1) into C(ion site)
             * (eq L-1 porous space), assuming density of solid is always
             * 2650 g L-1 */
            chms->t_conc[k] *= (1.0 - smcmax) * 2650.0;
            chms->p_conc[k] = chms->t_conc[k];
        }
        else
        {
            chms->t_conc[k] = t_conc[k];
            chms->p_actv[k] = chms->t_conc[k] * 0.5;
            chms->p_conc[k] = chms->t_conc[k] * 0.5;
            chms->ssa[k] = ssa[k];
        }
    }

    for (k = 0; k < rttbl->NumSsc; k++)
    {
        chms->s_conc[k] = ZERO_CONC;
    }

    /* Speciation */
    if (rttbl->RecFlg == KIN_REACTION)
    {
        _Speciation(chemtbl, rttbl, 1, chms);
    }

    /* Total moles should be calculated after speciation */
    for (k = 0; k < rttbl->NumStc; k++)
    {
        if (chemtbl[k].itype == AQUEOUS)
        {
            chms->t_mole[k] = chms->t_conc[k] * vol;
        }
        else
        {
            chms->t_mole[k] = 0.0;
        }
    }
}

double GWStrg(double depth, double smcmax, double smcmin, double gw)
{
    double          strg;

    if (gw < 0.0)
    {
        strg = 0.0;
    }
    else if (gw > depth)
    {
        strg = depth * smcmax + (gw - depth) * (smcmax - smcmin);
    }
    else
    {
        strg = gw * smcmax;
    }

    return strg;
}

double UnsatWaterStrg(double depth, double smcmax, double smcmin, double gw,
    double unsat)
{
    double          deficit;

    deficit = depth - gw;
    deficit = MIN(deficit, depth);
    deficit = MAX(deficit, 0.0);

    return deficit * smcmin + MAX(unsat, 0.0) * (smcmax - smcmin);
}

double UnsatSatRatio(double depth, double unsat, double gw)
{
    return ((unsat < 0.0) ? 0.0 : ((gw > depth) ? 1.0 : unsat / (depth - gw)));
}

double RivBedStrg(const matl_struct *matl, const river_wstate_struct *ws)
{
    double          strg;

    if (ws->gw < 0.0)
    {
        strg = 0.0;
    }
    else if (ws->gw > matl->bedthick)
    {
        strg = matl->bedthick * (matl->porosity + matl->smcmin) +
            (ws->gw - matl->bedthick) * matl->porosity;
    }
    else
    {
        strg = ws->gw * (matl->porosity + matl->smcmin);
    }

    return strg;
}
