namespace ConsoleApp3
{
    internal class Program
    {
        
        static void Main(string[] args)
        {
            Console.WriteLine("Hello, World!");
            string[] arr = { "a", "b", "c" };
            string b = "";
            string c = "";

            for (int i = 1; i <= 10; i++)
            {
                Console.Write(i + " ");   

                foreach (string item in arr)
                {   
                    Console.Write(item + " ");   
                    b= b+ item; 

                }
                c = c+"< b > " + b + " </ b >";
                Console.WriteLine("<b>" + b + "</b>"); 
                b = "";
            }
            Console.WriteLine(c);
        }
    }
}
