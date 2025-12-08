import java.rmi.registry.LocateRegistry;
import java.rmi.registry.Registry;
import java.util.Scanner;
import java.util.Arrays;

public class Client {

    public static void main(String[] args) {
        try {
            Registry registry = LocateRegistry.getRegistry("localhost", 1099);
            Gauss gauss = (Gauss) registry.lookup("GaussService");

            Scanner scanner = new Scanner(System.in);
            char option;

            System.out.println("1. 10");
            System.out.println("2. 100");
            System.out.println("3. 1000");
            System.out.println("4. 10000");
            System.out.println("5. test obliczen");
            System.out.println("q. quit");
            System.out.print("Wybierz opcję: ");

            while (true) {

                String input = scanner.nextLine();
                if (input.isEmpty()) continue;
                option = input.charAt(0);

                int size = 0;
                double[][] matrix;

                switch (option) {
                    case '1':
                        size = 10;
                        break;
                    case '2':
                        size = 100;
                        break;
                    case '3':
                        size = 1000;
                        break;
                    case '4':
                        size = 10000;
                        System.out.println("Poczekasz troche...");
                        break;
                    case '5':
                        // testowa macierz
                        double[][] testMatrix = {
                                {1, -2, 3, -7},
                                {3, 1, 4, 5},
                                {2, 5, 1, 18}
                        };

                        System.out.println("Wyniki dla testowej macierzy równolegle:");
                        double[][] solution = gauss.gaussianEliminationProcess(testMatrix);
                        MatrixUtils.printGaussSolutions(solution);

                        System.out.println("Wyniki dla testowej macierzy szeregowo:");
                        double[][] solution2 = gauss.gaussianElimination(testMatrix);
                        MatrixUtils.printGaussSolutions(solution2);
                        break;
                    case 'q':
                    case 'Q':
                        System.out.println("Koniec programu.");
                        return;
                    default:
                        continue;
                }

                if (size != 0) {
                    // generowanie macierzy losowej
                    matrix = MatrixUtils.createAugmentedMatrix(
                            MatrixUtils.generateDiagonallyDominantMatrix(size),
                            MatrixUtils.generateVectorB(size)
                    );

                    double[][] result = gauss.gaussianElimination(matrix);
                    double[][] resultp = gauss.gaussianEliminationProcess(matrix);
                    System.out.println("zwrócono macierz");
                    size = 0;
                }
            }

        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
