import java.rmi.RemoteException;
import java.rmi.server.UnicastRemoteObject;
import java.io.*;
import java.net.*;

public class GaussImpl extends UnicastRemoteObject implements Gauss {

    private static final int NUM_PROCS = 4;
    private static final int BASE_PORT = 5000;

    public GaussImpl() throws RemoteException {
        super();
    }

    @Override
    public String sayHello(String name) throws RemoteException {
        return "Witaj, " + name + "!";
    }

    // --- helper: zamiana wierszy ---
    private void swapRow(double[][] mat, int i, int j) {
        double[] tmp = mat[i];
        mat[i] = mat[j];
        mat[j] = tmp;
    }

    // --- forward elimination ---
    private int forwardElim(double[][] mat) {
        int N = mat.length;

        for (int k = 0; k < N; k++) {
            int iMax = k;
            double vMax = Math.abs(mat[iMax][k]);

            for (int i = k + 1; i < N; i++) {
                if (Math.abs(mat[i][k]) > vMax) {
                    vMax = Math.abs(mat[i][k]);
                    iMax = i;
                }
            }

            if (Math.abs(mat[iMax][k]) < 1e-9)
                return k;

            if (iMax != k)
                swapRow(mat, k, iMax);

            for (int i = k + 1; i < N; i++) {
                double f = mat[i][k] / mat[k][k];
                for (int j = k + 1; j <= N; j++)
                    mat[i][j] -= mat[k][j] * f;
                mat[i][k] = 0;
            }
        }
        return -1;
    }

    // --- back substitution ---
    private void backSub(double[][] mat) {
        int N = mat.length;
        double[] x = new double[N];

        for (int i = N - 1; i >= 0; i--) {
            x[i] = mat[i][N];
            for (int j = i + 1; j < N; j++)
                x[i] -= mat[i][j] * x[j];
            x[i] /= mat[i][i];
        }
        //for(int i=0; i<N; i++) {
        //    System.out.printf("%.4f\n", x[i]);
        // }
    }

    @Override
    public double[][] gaussianElimination(double[][] mat) throws RemoteException {

        System.out.println("\nrozmiar: " + mat.length);
        long startWall = System.nanoTime();
        long startCpu = System.nanoTime();  // uproszczenie — w Javie dokładny CPU time wymaga MXBean

        int singular = forwardElim(mat);

        if (singular != -1) {
            System.out.println("Macierz osobliwa — układ może nie mieć jednoznacznego rozwiązania.\n");
            return mat;
        }

        backSub(mat);

        long endWall = System.nanoTime();
        long endCpu = System.nanoTime();

        double wallSeconds = (endWall - startWall) / 1e9;
        double cpuSeconds = (endCpu - startCpu) / 1e9;

        System.out.println("szeregowo:");
        System.out.println("Czas zegarowy (wall): " + wallSeconds + " s");
        System.out.println("Czas CPU: " + cpuSeconds + " s");

        return mat;
    }

    @Override
    public double[][] gaussianEliminationProcess(double[][] mat) throws RemoteException {
        int N = mat.length;

        System.out.println("\nrozmiar: " + N);
        long startWall = System.nanoTime();
        long startCpu = System.nanoTime();  // uproszczenie — w Javie dokładny CPU time wymaga MXBean

        try {
            for (int k = 0; k < N; k++) {
                // Pivoting
                int iMax = k;
                double vMax = Math.abs(mat[iMax][k]);
                for (int i = k + 1; i < N; i++) {
                    if (Math.abs(mat[i][k]) > vMax) {
                        vMax = Math.abs(mat[i][k]);
                        iMax = i;
                    }
                }

                if (Math.abs(mat[iMax][k]) < 1e-9) {
                    System.out.println("Macierz osobliwa!");
                    return mat;
                }

                if (iMax != k) {
                    double[] temp = mat[k];
                    mat[k] = mat[iMax];
                    mat[iMax] = temp;
                }

                // Podział wierszy między procesy
                int rowsPerProc = (N - (k + 1) + NUM_PROCS - 1) / NUM_PROCS;
                ServerSocket[] servers = new ServerSocket[NUM_PROCS];
                Process[] procs = new Process[NUM_PROCS];

                // Tablice do komunikacji (wypełnione później)
                Socket[] sockets = new Socket[NUM_PROCS];
                ObjectOutputStream[] outs = new ObjectOutputStream[NUM_PROCS];
                ObjectInputStream[] ins = new ObjectInputStream[NUM_PROCS];

                // Najpierw utwórz wszystkie ServerSockety
                for (int p = 0; p < NUM_PROCS; p++) {
                    int start = k + 1 + p * rowsPerProc;
                    int end = Math.min(N, start + rowsPerProc);
                    if (start >= end) continue;

                    try {
                        servers[p] = new ServerSocket(BASE_PORT + p);
                    } catch (IOException e) {
                        System.err.println("Nie udało się utworzyć ServerSocket na porcie " + (BASE_PORT + p) + ": " + e.getMessage());
                        servers[p] = null;
                    }
                }

                // Teraz uruchom procesy worker
                for (int p = 0; p < NUM_PROCS; p++) {
                    int start = k + 1 + p * rowsPerProc;
                    int end = Math.min(N, start + rowsPerProc);
                    if (start >= end || servers[p] == null) continue;

                    try {
                        String classpath = System.getProperty("java.class.path");
                        ProcessBuilder pb = new ProcessBuilder("java", "-cp", classpath, "GaussWorker",
                                String.valueOf(k), String.valueOf(start), String.valueOf(end),
                                String.valueOf(BASE_PORT + p));
                        pb.inheritIO();
                        procs[p] = pb.start();
                    } catch (IOException e) {
                        System.err.println("Nie udało się uruchomić procesu GaussWorker: " + e.getMessage());
                        procs[p] = null;
                    }
                }

                // --- ETAP A: AKCEPTUJ WSZYSTKIE POŁĄCZENIA (nie blokujemy wysyłania/odbierania sekwencyjnie) ---
                for (int p = 0; p < NUM_PROCS; p++) {
                    int start = k + 1 + p * rowsPerProc;
                    int end = Math.min(N, start + rowsPerProc);
                    if (start >= end || servers[p] == null) continue;

                    try {
                        sockets[p] = servers[p].accept();
                        // Najpierw utwórz ObjectOutputStream, potem ObjectInputStream (zgodnie z konwencją)
                        outs[p] = new ObjectOutputStream(sockets[p].getOutputStream());
                        outs[p].flush();
                        ins[p] = new ObjectInputStream(sockets[p].getInputStream());
                    } catch (IOException e) {
                        System.err.println("Błąd accept() / streamów na porcie " + (BASE_PORT + p) + ": " + e.getMessage());
                        // spróbuj posprzątać ten gniazdo jeśli zostało otwarte
                        if (sockets[p] != null) {
                            try {
                                sockets[p].close();
                            } catch (IOException ignored) {
                            }
                            sockets[p] = null;
                        }
                    }
                }

                // --- ETAP B: WYŚLIJ DANE DO WSZYSTKICH WORKERÓW ---
                for (int p = 0; p < NUM_PROCS; p++) {
                    int start = k + 1 + p * rowsPerProc;
                    int end = Math.min(N, start + rowsPerProc);
                    if (start >= end || outs[p] == null) continue;

                    try {
                        // Wyślij pivot
                        outs[p].writeObject(mat[k]);
                        outs[p].flush();

                        // Wyślij wiersze do przetworzenia
                        for (int i = start; i < end; i++) {
                            outs[p].writeObject(mat[i]);
                            outs[p].flush();
                        }
                    } catch (IOException e) {
                        System.err.println("Błąd wysyłania danych do workera " + p + " na porcie " + (BASE_PORT + p) + ": " + e.getMessage());
                        // Nie zamykamy tu całego procesu — sprzątamy później
                    }
                }

                // --- ETAP C: ODBIERZ WYNIKI OD WSZYSTKICH WORKERÓW ---
                for (int p = 0; p < NUM_PROCS; p++) {
                    int start = k + 1 + p * rowsPerProc;
                    int end = Math.min(N, start + rowsPerProc);
                    if (start >= end || ins[p] == null) continue;

                    try {
                        for (int i = start; i < end; i++) {
                            double[] updatedRow = (double[]) ins[p].readObject();
                            mat[i] = updatedRow;
                        }
                    } catch (IOException | ClassNotFoundException e) {
                        System.err.println("Błąd odbierania danych od workera " + p + " na porcie " + (BASE_PORT + p) + ": " + e.getMessage());
                    }
                }

                // --- SPRZĄTANIE: zamknij strumienie, sockety i serwery ---
                for (int p = 0; p < NUM_PROCS; p++) {
                    try {
                        if (ins[p] != null) {
                            ins[p].close();
                            ins[p] = null;
                        }
                    } catch (IOException ignored) {
                    }
                    try {
                        if (outs[p] != null) {
                            outs[p].close();
                            outs[p] = null;
                        }
                    } catch (IOException ignored) {
                    }
                    try {
                        if (sockets[p] != null) {
                            sockets[p].close();
                            sockets[p] = null;
                        }
                    } catch (IOException ignored) {
                    }
                    try {
                        if (servers[p] != null) {
                            servers[p].close();
                            servers[p] = null;
                        }
                    } catch (IOException e) {
                        System.err.println("Błąd zamykania ServerSocket dla p=" + p + ": " + e.getMessage());
                    }
                }

                // Czekamy na zakończenie wszystkich procesów worker
                for (Process p : procs) {
                    if (p != null) {
                        try {
                            p.waitFor();
                        } catch (InterruptedException e) {
                            System.err.println("Proces worker został przerwany: " + e.getMessage());
                            Thread.currentThread().interrupt();
                        }
                    }
                }
            }

            // Back substitution
            double[] x = new double[N];
            for (int i = N - 1; i >= 0; i--) {
                x[i] = mat[i][N];
                for (int j = i + 1; j < N; j++)
                    x[i] -= mat[i][j] * x[j];
                x[i] /= mat[i][i];
            }

            long endWall = System.nanoTime();
            long endCpu = System.nanoTime();

            double wallSeconds = (endWall - startWall) / 1e9;
            double cpuSeconds = (endCpu - startCpu) / 1e9;

            System.out.println("procesy:");
            System.out.println("Czas zegarowy (wall): " + wallSeconds + " s");
            System.out.println("Czas CPU: " + cpuSeconds + " s");

        } catch (Exception e) {
            System.err.println("Wystąpił błąd w gaussianEliminationProcess: " + e.getMessage());
            e.printStackTrace();
        }

        return mat;
    }
}



