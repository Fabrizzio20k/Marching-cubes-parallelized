import sys
from pathlib import Path
import numpy as np
import pandas as pd
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

plt.rcParams.update({"figure.figsize": (6, 4.2), "axes.grid": True,
                     "grid.alpha": 0.3, "font.size": 11})


def save(fig, outdir, name):
    fig.tight_layout()
    fig.savefig(outdir / f"{name}.png", dpi=200)
    fig.savefig(outdir / f"{name}.pdf")
    plt.close(fig)


def latex_table(mpi, t1_total, path):
    lines = [
        r"\begin{table}[htbp]",
        r"\caption{Resultados de escalabilidad (mediana de las repeticiones)}",
        r"\label{tab:resultados}",
        r"\centering",
        r"\begin{tabular}{rrrrrrr}",
        r"\hline",
        r"$p$ & $T_{comp}$ (s) & $T_{IO}$ (s) & $T_{total}$ (s) & $S_p$ & $E_p$ & GFLOPS \\",
        r"\hline",
    ]
    for _, r in mpi.iterrows():
        sp = t1_total / r.t_total
        ep = sp / r.p
        lines.append(
            f"{int(r.p)} & {r.t_computo:.3f} & {r.t_io:.3f} & "
            f"{r.t_total:.3f} & {sp:.2f} & {ep:.2f} & {r.gflops:.2f} \\\\"
        )
    lines += [r"\hline", r"\end{tabular}", r"\end{table}"]
    path.write_text("\n".join(lines) + "\n")


def plot_group(N, sub, outdir):
    seq = sub[sub.version == "seq"]
    mpi = sub[sub.version == "mpi"].sort_values("p")
    if mpi.empty:
        return
    base = seq if not seq.empty else mpi[mpi.p == 1]
    t1_comp = float(base.t_computo.iloc[0])
    t1_total = float(base.t_total.iloc[0])

    p = mpi.p.to_numpy(dtype=float)
    s_comp = t1_comp / mpi.t_computo.to_numpy()
    s_total = t1_total / mpi.t_total.to_numpy()

    fig, ax = plt.subplots()
    ax.plot(p, p, "k--", label="Ideal ($S_p = p$)")
    ax.plot(p, s_comp, "o-", label="Cómputo")
    ax.plot(p, s_total, "s-", label="Total (cómputo + I/O)")
    ax.set_xlabel("Número de procesos $p$")
    ax.set_ylabel("Speedup $S_p$")
    ax.set_title(f"Speedup vs. procesos (N={N})")
    ax.set_xticks(p)
    ax.legend()
    save(fig, outdir, f"speedup_N{N}")

    fig, ax = plt.subplots()
    ax.axhline(1.0, color="k", linestyle="--", label="Ideal ($E_p = 1$)")
    ax.plot(p, s_comp / p, "o-", label="Cómputo")
    ax.plot(p, s_total / p, "s-", label="Total")
    ax.set_xlabel("Número de procesos $p$")
    ax.set_ylabel("Eficiencia $E_p$")
    ax.set_title(f"Eficiencia vs. procesos (N={N})")
    ax.set_xticks(p)
    ax.set_ylim(0, 1.15)
    ax.legend()
    save(fig, outdir, f"eficiencia_N{N}")

    fig, ax = plt.subplots()
    ax.plot(p, mpi.gflops, "o-", label="MPI")
    if not seq.empty:
        ax.axhline(float(seq.gflops.iloc[0]), color="gray", linestyle="--",
                   label="Secuencial")
    ax.set_xlabel("Número de procesos $p$")
    ax.set_ylabel("GFLOPS")
    ax.set_title(f"Rendimiento computacional vs. procesos (N={N})")
    ax.set_xticks(p)
    ax.legend()
    save(fig, outdir, f"gflops_N{N}")

    fig, ax = plt.subplots()
    x = np.arange(len(p))
    ax.bar(x, mpi.t_computo, label="Cómputo")
    ax.bar(x, mpi.t_io, bottom=mpi.t_computo, label="Escritura (MPI-IO)")
    ax.set_xticks(x)
    ax.set_xticklabels([int(v) for v in p])
    ax.set_xlabel("Número de procesos $p$")
    ax.set_ylabel("Tiempo (s)")
    ax.set_title(f"Descomposición del tiempo de ejecución (N={N})")
    ax.legend()
    save(fig, outdir, f"tiempos_N{N}")

    fig, ax = plt.subplots()
    ratio = mpi.t_comp_max.to_numpy() / mpi.t_comp_min.to_numpy()
    ax.plot(p, ratio, "o-")
    ax.axhline(1.0, color="k", linestyle="--")
    ax.set_xlabel("Número de procesos $p$")
    ax.set_ylabel("$T_{max} / T_{min}$ por rank")
    ax.set_title(f"Balance de carga (N={N})")
    ax.set_xticks(p)
    save(fig, outdir, f"balance_N{N}")

    mask = p > 1
    if mask.any():
        pk = p[mask]
        sk = s_total[mask]
        kf = (1 / sk - 1 / pk) / (1 - 1 / pk)
        fig, ax = plt.subplots()
        ax.plot(pk, kf, "o-")
        ax.set_xlabel("Número de procesos $p$")
        ax.set_ylabel("Fracción serial experimental $e$")
        ax.set_title(f"Métrica de Karp-Flatt (N={N})")
        ax.set_xticks(pk)
        save(fig, outdir, f"karp_flatt_N{N}")

    latex_table(mpi, t1_total, outdir / f"tabla_N{N}.tex")

    resumen = mpi.assign(
        speedup_comp=s_comp, speedup_total=s_total,
        eficiencia_comp=s_comp / p, eficiencia_total=s_total / p,
        balance=mpi.t_comp_max / mpi.t_comp_min,
    )
    resumen.to_csv(outdir / f"resumen_N{N}.csv", index=False)
    print(f"N={N}: T1={t1_total:.3f}s, "
          f"S_max={s_total.max():.2f} con p={int(p[s_total.argmax()])}")


def main():
    csv = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("results.csv")
    outdir = Path(sys.argv[2]) if len(sys.argv) > 2 else Path(__file__).parent / "figures"
    outdir.mkdir(parents=True, exist_ok=True)

    df = pd.read_csv(csv)
    agg = df.groupby(["version", "p", "N"], as_index=False).median(numeric_only=True)

    for N, sub in agg.groupby("N"):
        plot_group(N, sub, outdir)

    print(f"Figuras y tablas generadas en {outdir}/")


if __name__ == "__main__":
    main()
