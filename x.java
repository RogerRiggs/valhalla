//package example
import java.util.Map;
import java.util.HashMap;
import java.util.function.Function;
import java.util.function.BiFunction;
import java.util.Arrays;
import java.lang.reflect.Constructor;

public inline class x {
    int i = 0;
    public static void main(String[] args) {
        x a = new x();
        for (int i = 0; i < 1; i++) {
//            eq0();
//            eq3();
//            f0();
//            f1();
//            f1a();
//            f2();
//            f3();
            f4(5);
        }
    }

    // Static factory initq
    x() { }

    static void f0() {
        Runnable r = () -> System.out.print("");
        r.run();
    }
    static void eq0() {
        Runnable r = () -> System.out.print("");
        Runnable r1 = () -> System.out.print("");
        System.out.println("inline: " + r.getClass().isInlineClass());
        System.out.println("r.equals(r): " + r.equals(r) +
                ", hashcode: " + r.hashCode());
        System.out.println("inline: " + r1.getClass().isInlineClass());
        System.out.println("r.equals(r1): " + r.equals(r1) +
                ", r1.hashcode: " + r1.hashCode());
    }
    static void f1() {
        final String delim = delim();
        Function<String, String> f = (s) -> delim + s + delim;
        String s = f.apply("f1");
//        System.out.println(s);
    }

    static void f1a() {
        final String delim = delim();
        Function<String, String> f = i -> i;
        String s = f.apply("f1");
//        System.out.println(s);
    }

    static void f2() {
        final String delim = delim();
        BiFunction<String, String, String> f2 = (a, b) -> delim + a + "@" + b + delim;
        String s = f2.apply("f1", "f2");
//        System.out.println(s);
    }

    static void f3() {
        final String delim = delim();
        final Object nl = System.lineSeparator();
        BiFunction<String, String, String> f2 = (a, b) -> delim + a + "@" + b + delim + nl;
        String s = f2.apply("f1", "f2");
//        System.out.println(s);
    }
    static void eq3() {
        final String delim = delim();
        final Object nl = "EOL";
        BiFunction<String, String, String> f2 = (a, b) -> delim + a + "@" + b + delim + nl;
        BiFunction<String, String, String> f2a = (a, b) -> delim + a + "@" + b + delim + nl;
        System.out.println("f2 == f2a: " + (f2 == f2a));
        String s = f2.apply("f1", "f2");
        String s2 = f2a.apply("f1", "f2");
        System.out.println(printInlineClass(f2));
        System.out.println("f2.equals(f2): " + f2.equals(f2) +
                ", f2.hashcode: " + f2.hashCode());
        System.out.println(printInlineClass(f2a));
        System.out.println("f2.equals(f2a): " + f2.equals(f2a) +
                ", f2.hashcode: " + f2.hashCode() +
                ", f2a.hashcode: " + f2a.hashCode());
        System.out.println(s);
        System.out.println(s2);
    }

    static void f4(int delta) {
        int num = delta;
        BiFunction <Integer,Integer, Integer> f = (a, b) -> a + b + num;

        Map<Object, Integer> m = new java.util.HashMap<>();
        for (int i = 0; i < 5; i++) {
            System.out.println("m.get(f): " + m.get(f));
            var v = f.apply(i, 2);
            System.out.println("i: " + i + ", v: " + v);
            m.put(f, i);
            if (m.get(f) != i) {
                System.out.println("!=: " + i);
            }
        }
        System.out.println(m);
    }

    static String printInlineClass(Object o) {
        Class<?> cl = o.getClass();
        return cl.toString() + ", inline: " + cl.isInlineClass() +
                ", methods: " + Arrays.toString(cl.getDeclaredMethods()) +
                ", toString(): " + o.toString();
    }
    static String delim() {
        return "|";
    }

    static void t0() {
        Constructor<?>[] c = x.class.getDeclaredConstructors();
        System.out.println("c[]: " + Arrays.toString(c));
    }
}
