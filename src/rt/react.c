/******************************************************************************
* This subroutine is used to calculate the reactions
* It uses a similar OS3D scheme as detailed inCrunchflow user's manual.
*
* If you have any questions, concerns, suggestions, please contact me at
* the following address:
*     Developer: Chen Bao <baochen.d.s@gmail.com>
*     Version  : pre-alpha
*     Date     : June, 2013
*****************************************************************************/
#include "pihm.h"

#define LINE_WIDTH 512
#define WORDS_LINE 40
#define WORD_WIDTH 80
#define TOL        1E-7
#define SKIP_JACOB 1
#define sqr(a)  (a)*(a)

void Reaction(double stepsize, const chemtbl_struct chemtbl[],
    const kintbl_struct kintbl[], const rttbl_struct *rttbl, elem_struct elem[])
{
    int             i;

    for (i = 0; i < nelem; i++)
    {
        double          t_conc0[MAXSPS];
        double          substep;
        double          vol_gw;
        double          vol_unsat;
        double          satn;
        int             j, k;
        int             illness;

        vol_gw = GWVol(&elem[i].topo, &elem[i].soil, &elem[i].ws);
        vol_unsat = UnsatWaterVol(&elem[i].topo, &elem[i].soil, &elem[i].ws);
        satn =
            UnsatSatRatio(elem[i].soil.depth, elem[i].ws.unsat, elem[i].ws.gw);

        for (k = 0; k < NumSpc; k++)
        {
            t_conc0[k] = elem[i].chms_unsat.t_conc[k];
        }

        illness = 0;
        if (illness < 20 && elem[i].ws.gw > 1.0E-3)
        {
            if (_React(stepsize, chemtbl, kintbl, rttbl, satn, &illness,
                &elem[i].chms_unsat))
            {
                fprintf(stderr, "  ---> React failed.\t");

                substep = 0.5 * stepsize;
                k = 2;

                while ((j = _React(substep, chemtbl, kintbl, rttbl, satn, &illness,
                    &elem[i].chms_unsat)))
                {
                    substep *= 0.5;
                    k *= 2;
                    if (substep < 30.0)
                        break;
                }

                if (j == 0)
                {
                    fprintf(stderr,
                            " Reaction passed with step equals to %f (1/%d)\n",
                            substep, k);
                    for (j = 1; j < k; j++)
                    {
                        _React(substep, chemtbl, kintbl, rttbl, satn, &illness,
                            &elem[i].chms_unsat);
                    }
                }
            }
        }   /* Finish reaction */

        for (k = 0; k < NumSpc; k++)
        {
            elem[i].chmf.react_unsat[k] =
                (elem[i].chms_unsat.t_conc[k] - t_conc0[k]) * vol_unsat / stepsize;
        }

        /*
         * Groundwater
         */
        for (k = 0; k < NumSpc; k++)
        {
            t_conc0[k] = elem[i].chms_gw.t_conc[k];
        }

        illness = 0;
        if (illness < 20 && elem[i].ws.gw > 1.0E-3)
        {
            if (_React(stepsize, chemtbl, kintbl, rttbl, 1.0, &illness,
                &elem[i].chms_gw))
            {
                fprintf(stderr, "  ---> React failed.\t");

                substep = 0.5 * stepsize;
                k = 2;

                while ((j = _React(substep, chemtbl, kintbl, rttbl, 1.0, &illness,
                    &elem[i].chms_gw)))
                {
                    substep *= 0.5;
                    k *= 2;
                    if (substep < 30.0)
                        break;
                }

                if (j == 0)
                {
                    fprintf(stderr,
                            " Reaction passed with step equals to %f (1/%d)\n",
                            substep, k);
                    for (j = 1; j < k; j++)
                    {
                        _React(substep, chemtbl, kintbl, rttbl, satn, &illness,
                            &elem[i].chms_gw);
                    }
                }
            }
        }   /* Finish reaction */

        for (k = 0; k < NumSpc; k++)
        {
            elem[i].chmf.react_gw[k] =
                (elem[i].chms_gw.t_conc[k] - t_conc0[k]) * vol_gw / stepsize;
        }

        /* Averaging mineral concentration to ensure mass
         * conservation !! */
        for (k = 0; k < rttbl->NumStc; k++)
        {
            if (chemtbl[k].itype == MINERAL)
            {
                elem[i].chms_gw.t_conc[k] =
                    (elem[i].chms_gw.t_conc[k] * vol_gw +
                    elem[i].chms_unsat.t_conc[k] * vol_unsat) /
                    (vol_gw + vol_unsat);
                elem[i].chms_unsat.t_conc[k] =
                    elem[i].chms_gw.t_conc[k];
                elem[i].chms_gw.p_conc[k] =
                    elem[i].chms_gw.t_conc[k];
                elem[i].chms_unsat.p_conc[k] =
                    elem[i].chms_gw.t_conc[k];
            }
        }
    }
}

int _React(double stepsize, const chemtbl_struct chemtbl[],
    const kintbl_struct kintbl[], const rttbl_struct *rttbl, double satn,
    int *illness, chmstate_struct *chms)
{
    if (satn < 1.0E-2)
    {
        /* very dry, no reaction can take place */
        return (0);
    }
    int             i, j, k, control, num_spe =
        rttbl->NumStc + rttbl->NumSsc, min_pos, pivot_flg;
    int             mn, in;
    double          monodterm = 1.0, inhibterm = 1.0;
    int             stc = rttbl->NumStc, ssc = rttbl->NumSsc, nkr =
        rttbl->NumMkr + rttbl->NumAkr, smc = rttbl->NumMin;
    double         *residue, *residue_t, *tmpconc, *totconc, *area, *error,
        *gamma, *Keq, *Rate_pre, *IAP, *dependency, *Rate_spe, *Rate_spe_t,
        *Rate_spet;
    const int       SUFEFF = 1;
    residue = (double *)malloc(stc * sizeof(double));
    residue_t = (double *)malloc(stc * sizeof(double));
    tmpconc = (double *)malloc(num_spe * sizeof(double));
    totconc = (double *)malloc(stc * sizeof(double));
    area = (double *)malloc(smc * sizeof(double));
    error = (double *)malloc(stc * sizeof(double));
    gamma = (double *)malloc(num_spe * sizeof(double));
    Keq = (double *)malloc(ssc * sizeof(double));
    Rate_pre = (double *)malloc(nkr * sizeof(double));
    IAP = (double *)malloc(nkr * sizeof(double));
    dependency = (double *)malloc(nkr * sizeof(double));
    Rate_spe = (double *)malloc(stc * sizeof(double));
    Rate_spe_t = (double *)malloc(stc * sizeof(double));
    Rate_spet = (double *)malloc(stc * sizeof(double));
    long int       *p =
        (long int *)malloc((rttbl->NumStc - rttbl->NumMin) * sizeof(long int));
    realtype       *x_ =
        (realtype *)malloc((rttbl->NumStc - rttbl->NumMin) * sizeof(realtype));
    double          tmpval, tmpprb, inv_sat, I, Iroot, tmpKeq, adh,
        bdh, bdt, maxerror = 1, surf_ratio, tot_cec, tmpprb_inv;
    realtype      **jcb;

    /* Build model data structure from pointer address */
    control = 0;
    tmpprb = 1.0E-2;
    tmpprb_inv = 1.0 / tmpprb;
    inv_sat = 1.0 / satn;

    for (i = 0; i < rttbl->NumMin; i++)
    {
        area[i] = chms->ssa[i + rttbl->NumStc - rttbl->NumMin] *
            chms->p_conc[i + rttbl->NumStc - rttbl->NumMin] *
            chemtbl[i + rttbl->NumStc - rttbl->NumMin].MolarMass;
    }

    if (SUFEFF)
    {
        if (satn < 1.0)
        {
            //surf_ratio = 1.0;  /* # 1 function */
            surf_ratio = exp(satn) - 1.0;    /* # 3 function */
            //surf_ratio = 1.0 - pow(exp(-1.0/satn), 0.6); /* # 4 function */
            for (i = 0; i < rttbl->NumMin; i++)
            {
                area[i] *= surf_ratio;
            }
        }
    }   /* Lichtner's 2 third law if SUF_EFF is turned on. */

    for (j = 0; j < rttbl->NumStc; j++)
    {
        Rate_spe[j] = 0.0;
    }

    for (i = 0; i < rttbl->NumMkr + rttbl->NumAkr; i++)
    {
        min_pos = kintbl[i].position - rttbl->NumStc + rttbl->NumMin;

        if (kintbl[i].type == 1)  /* TST rate */
        {
            IAP[i] = 0.0;
            for (j = 0; j < rttbl->NumStc; j++)
            {
                IAP[i] += log10(chms->p_actv[j]) *
                    rttbl->Dep_kinetic[min_pos][j];
            }
            IAP[i] = pow(10, IAP[i]);
            tmpKeq = pow(10, rttbl->KeqKinect[min_pos]);
            dependency[i] = 1.0;
            for (k = 0; k < kintbl[i].num_dep; k++)
                dependency[i] *=
                    pow(chms->p_actv[kintbl[i].dep_position[k]],
                    kintbl[i].dep_power[k]);
            /* Calculate the predicted rate depending on the type of rate law!  */
            Rate_pre[i] = area[min_pos] * (pow(10, kintbl[i].rate)) *
                dependency[i] * (1 - (IAP[i] / tmpKeq));
            /* Rate_pre: rate per reaction (mol (L water)-1 s-1)
             * area: m2/L water
             * rate: mol/m2/s
             * dependency: dimensionless */
        }
        else if (kintbl[i].type == 4) /* Monod rate */
        {
            monodterm = 1.0;    /* re-set for new species */
            inhibterm = 1.0;    /*re-set for new species */

            /* Calculate rate */
            for (mn = 0; mn < kintbl[i].num_monod; mn++)
            {
                monodterm *=
                    chms->p_conc[kintbl[i].monod_position[mn]] /
                    (chms->p_conc[kintbl[i].monod_position[mn]] +
                    kintbl[i].monod_para[mn]);
            }

            for (in = 0; in < kintbl[i].num_inhib; in++)
            {
                inhibterm *= kintbl[i].inhib_para[in] /
                    (kintbl[i].inhib_para[in] +
                    chms->p_conc[kintbl[i].inhib_position[in]]);
            }

            /* Based on CrunchTope */
            Rate_pre[i] = area[min_pos] * pow(10, kintbl[i].rate) * monodterm;
        }

        for (j = 0; j < rttbl->NumStc; j++)
        {
            Rate_spe[j] += Rate_pre[i] * rttbl->Dep_kinetic[min_pos][j];
        }
    }

    for (i = 0; i < rttbl->NumMkr + rttbl->NumAkr; i++)
    {
        min_pos = kintbl[i].position - rttbl->NumStc + rttbl->NumMin;
        if (Rate_pre[i] < 0.0)
        {
            if (chms->p_conc[min_pos + rttbl->NumStc - rttbl->NumMin] < 1.0E-8) /* mineral cutoff when mineral is disappearing */
                area[min_pos] = 0.0;
        }
    }

    for (i = 0; i < NumSpc; i++)
        if (chemtbl[i].itype == AQUEOUS) /* 01.21 aqueous species, saturation term for aqueous volume */
            Rate_spe[i] *= inv_sat;

    jcb = newDenseMat(rttbl->NumStc - rttbl->NumMin, rttbl->NumStc - rttbl->NumMin);

    if (rttbl->TEMcpl == 0)
    {
        for (i = 0; i < rttbl->NumSsc; i++)
            Keq[i] = rttbl->Keq[i];
    }

    adh = rttbl->adh;
    bdh = rttbl->bdh;
    bdt = rttbl->bdt;

    for (i = 0; i < rttbl->NumStc; i++)
    {
        tmpconc[i] = log10(chms->p_conc[i]);
    }
    for (i = 0; i < rttbl->NumSsc; i++)
    {
        tmpconc[i + rttbl->NumStc] = log10(chms->s_conc[i]);
    }
    tot_cec = 0.0;
    for (i = 0; i < num_spe; i++)
    {
        if (chemtbl[i].itype == CATION_ECHG)
        {
            tot_cec += pow(10, tmpconc[i]);
        }
    }

    I = 0;
    for (i = 0; i < num_spe; i++)
    {
        I += 0.5 * pow(10, tmpconc[i]) * sqr(chemtbl[i].Charge);
    }
    Iroot = sqrt(I);
    for (i = 0; i < num_spe; i++)
    {
        switch (chemtbl[i].itype)
        {
            case AQUEOUS:
                gamma[i] =
                    (-adh * sqr(chemtbl[i].Charge) * Iroot) / (1 +
                    bdh * chemtbl[i].SizeF * Iroot) + bdt * I;
                break;
            case ADSORPTION:
                gamma[i] = log10(satn);
                break;
            case CATION_ECHG:
                gamma[i] = -log10(tot_cec);
                break;
            case MINERAL:
                gamma[i] = -tmpconc[i];
                break;
        }
    }

    while (maxerror > TOL)
    {
        for (i = 0; i < rttbl->NumSsc; i++)
        {
            tmpval = 0.0;
            for (j = 0; j < rttbl->NumSdc; j++)
            {
                tmpval += (tmpconc[j] + gamma[j]) * rttbl->Dependency[i][j];
            }
            tmpval -= Keq[i] + gamma[i + rttbl->NumStc];
            tmpconc[i + rttbl->NumStc] = tmpval;
        }

        for (j = 0; j < rttbl->NumStc; j++)
        {
            Rate_spet[j] = 0.0;
        }

        for (i = 0; i < rttbl->NumMkr + rttbl->NumAkr; i++)
        {
            min_pos = kintbl[i].position - rttbl->NumStc + rttbl->NumMin;

            if (kintbl[i].type == 1)  /* TST rate */
            {
                IAP[i] = 0.0;
                for (j = 0; j < rttbl->NumStc; j++)
                {
                    if (chemtbl[j].itype != MINERAL)
                    {
                        IAP[i] += (tmpconc[j] + gamma[j]) *
                            rttbl->Dep_kinetic[min_pos][j];
                    }
                }
                IAP[i] = pow(10, IAP[i]);
                tmpKeq = pow(10, rttbl->KeqKinect[min_pos]);
                /*
                 * if ( IAP[i] < tmpKeq)
                 * rct_drct[i] = 1.0;
                 * if ( IAP[i] > tmpKeq)
                 * rct_drct[i] = -1.0;
                 * if ( IAP[i] == tmpKeq)
                 * rct_drct[i] = 0.0;
                 */
                dependency[i] = 0.0;
                for (k = 0; k < kintbl[i].num_dep; k++)
                    dependency[i] +=
                        (tmpconc[kintbl[i].dep_position[k]] +
                        gamma[kintbl[i].dep_position[k]]) *
                        kintbl[i].dep_power[k];
                dependency[i] = pow(10, dependency[i]);
                /* Calculate the predicted rate depending on the type of rate law!  */
                Rate_pre[i] = area[min_pos] * (pow(10, kintbl[i].rate)) *
                    dependency[i] * (1 - (IAP[i] / tmpKeq));
                /* Rate_pre: in mol / L water / s
                 * area: m2/L water
                 * rate: mol/m2/s
                 * dependency: dimensionless;
                 */
            }
            else if (kintbl[i].type == 4)
            {
                monodterm = 1.0;
                inhibterm = 1.0;

                /* Calculate rate */
                for (mn = 0; mn < kintbl[i].num_monod; mn++)
                {
                    monodterm *=
                        chms->p_conc[kintbl[i].monod_position[mn]] /
                        (chms->p_conc[kintbl[i].monod_position[mn]] +
                        kintbl[i].monod_para[mn]);
                }

                for (in = 0; in < kintbl[i].num_inhib; in++)
                {
                    inhibterm *=
                        kintbl[i].inhib_para[in] /
                        (kintbl[i].inhib_para[in] +
                        chms->p_conc[kintbl[i].inhib_position[in]]);
                }

                /* Based on CrunchTope */
                Rate_pre[i] =
                    area[min_pos] * pow(10, kintbl[i].rate) * monodterm;
            }

            for (j = 0; j < rttbl->NumStc; j++)
            {
                Rate_spet[j] += Rate_pre[i] * rttbl->Dep_kinetic[min_pos][j];
            }
            /* Adjust the unit of the calcuated rate. Note that for mineral, the
             * unit of rate and the unit of concentration are mol/L porous media
             * For the aqueous species, the unit of the rate and the unit of the
             * concentration are mol/L pm and mol/L water respectively. */
        }

        for (i = 0; i < NumSpc; i++)
            if (chemtbl[i].itype == AQUEOUS)
                Rate_spet[i] *= inv_sat;

        for (i = 0; i < rttbl->NumStc - rttbl->NumMin; i++)
        {
            tmpval = 0.0;
            for (j = 0; j < rttbl->NumStc + rttbl->NumSsc; j++)
            {
                tmpval += rttbl->Totalconc[i][j] * pow(10, tmpconc[j]);
            }
            totconc[i] = tmpval;
            residue[i] = tmpval - (chms->t_conc[i] +
                (Rate_spe[i] + Rate_spet[i]) * stepsize * 0.5);
        }
        if (control % SKIP_JACOB == 0)  /* update jacobian every the other iteration */
        {
            for (k = 0; k < rttbl->NumStc - rttbl->NumMin; k++)
            {
                tmpconc[k] += tmpprb;
                for (i = 0; i < rttbl->NumSsc; i++)
                {
                    tmpval = 0.0;
                    for (j = 0; j < rttbl->NumSdc; j++)
                        tmpval +=
                            (tmpconc[j] + gamma[j]) * rttbl->Dependency[i][j];
                    tmpval -= Keq[i] + gamma[i + rttbl->NumStc];
                    tmpconc[i + rttbl->NumStc] = tmpval;
                }
                for (i = 0; i < rttbl->NumStc - rttbl->NumMin; i++)
                {
                    tmpval = 0.0;
                    for (j = 0; j < rttbl->NumStc + rttbl->NumSsc; j++)
                    {
                        tmpval += rttbl->Totalconc[i][j] * pow(10, tmpconc[j]);
                    }
                    residue_t[i] = tmpval - (chms->t_conc[i] +
                        (Rate_spe[i] + Rate_spet[i]) * stepsize * 0.5);
                    jcb[k][i] = (residue_t[i] - residue[i]) * tmpprb_inv;
                }
                tmpconc[k] -= tmpprb;
            }
        }
        for (i = 0; i < rttbl->NumStc - rttbl->NumMin; i++)
            x_[i] = -residue[i];

        pivot_flg = denseGETRF(jcb, rttbl->NumStc - rttbl->NumMin, rttbl->NumStc - rttbl->NumMin, p);   // 09.17
        if (pivot_flg != 0)
        {
            (*illness)++;
            return (1);
        }

        denseGETRS(jcb, rttbl->NumStc - rttbl->NumMin, p, x_);

        for (i = 0; i < rttbl->NumStc - rttbl->NumMin; i++)
        {
            if (fabs(x_[i]) < 0.3)
                tmpconc[i] += x_[i];
            else
            {
                if (x_[i] < 0)
                    tmpconc[i] += -0.3;
                else
                    tmpconc[i] += 0.3;
            }
            error[i] = residue[i] / totconc[i];
        }
        maxerror = fabs(error[0]);
        for (i = 1; i < rttbl->NumStc - rttbl->NumMin; i++)
            if (fabs(error[i]) > maxerror)
                maxerror = fabs(error[i]);
        control++;
        if (control > 10)
            return (1);
    }

    destroyMat(jcb);

    for (i = 0; i < rttbl->NumSsc; i++)
    {
        tmpval = 0.0;
        for (j = 0; j < rttbl->NumSdc; j++)
        {
            tmpval += (tmpconc[j] + gamma[j]) * rttbl->Dependency[i][j];
        }
        tmpval -= Keq[i] + gamma[i + rttbl->NumStc];
        tmpconc[i + rttbl->NumStc] = tmpval;
    }

    for (i = 0; i < rttbl->NumStc - rttbl->NumMin; i++)
    {
        tmpval = 0.0;
        for (j = 0; j < rttbl->NumStc + rttbl->NumSsc; j++)
        {
            tmpval += rttbl->Totalconc[i][j] * pow(10, tmpconc[j]);
        }
        totconc[i] = tmpval;
        residue[i] = tmpval - chms->t_conc[i];
        error[i] = residue[i] / totconc[i];
    }
    for (i = 0; i < rttbl->NumStc + rttbl->NumSsc; i++)
    {
        if (i < rttbl->NumStc)
        {
            if (chemtbl[i].itype == MINERAL)
            {
                chms->t_conc[i] +=
                    (Rate_spe[i] + Rate_spet[i]) * stepsize * 0.5;
                chms->p_actv[i] = 1.0;
                chms->p_conc[i] = chms->t_conc[i];
            }
            else
            {
                chms->p_conc[i] = pow(10, tmpconc[i]);
                chms->p_actv[i] = pow(10, (tmpconc[i] + gamma[i]));
                chms->t_conc[i] = totconc[i];
            }
        }
        else
        {
            chms->s_conc[i - rttbl->NumStc] = pow(10, tmpconc[i]);
#if TEMP_DISABLED
            chms->s_actv[i - rttbl->NumStc] = pow(10, (tmpconc[i] + gamma[i]));
#endif
        }
    }

    free(residue);
    free(residue_t);
    free(tmpconc);
    free(totconc);
    free(area);
    free(error);
    free(gamma);
    free(Keq);
    free(Rate_pre);
    free(IAP);
    free(dependency);
    free(Rate_spe);
    free(Rate_spe_t);
    free(Rate_spet);
    free(p);
    free(x_);
    return (0);
}


