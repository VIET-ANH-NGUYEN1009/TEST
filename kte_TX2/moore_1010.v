module moore_1010 (
    input wire din, clk, rst,
    output reg dout
);
    localparam [2:0]
        s0 = 3'd0,  // trạng thái ban đầu
        s1 = 3'd1,  // nhận '1'
        s2 = 3'd2,  // nhận '10'
        s3 = 3'd3,  // nhận '101'
        s4 = 3'd4;  // nhận '1010' -> phát hiện

    reg [2:0] state, next_state;

    // State register
    always @(posedge clk or posedge rst) begin
        if (rst)
            state <= s0;
        else
            state <= next_state;
    end

    // Next-state logic
    always @(*) begin
        case (state)
            s0: if (din) next_state = s1; else next_state = s0;
            s1: if (!din) next_state = s2; else next_state = s1;
            s2: if (din) next_state = s3; else next_state = s0;
            s3: if (!din) next_state = s4; else next_state = s1;
            s4: if (din) next_state = s1; else next_state = s2;
            default: next_state = s0;
        endcase
    end

    // Output logic (Moore)
    always @(*) begin
        case (state)
            s4: dout = 1'b1;  // phát hiện chuỗi 1010
            default: dout = 1'b0;
        endcase
    end
endmodule

********************************************
`timescale 1ns / 1ps

module tb_moore_1010;
    reg clk, rst, din;
    wire dout;

    moore_1010 uut (
        .clk(clk),
        .rst(rst),
        .din(din),
        .dout(dout)
    );

    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end

    initial begin
        $monitor("Time=%0t | din=%b | dout=%b", $time, din, dout);
        rst = 1; din = 0;
        #10 rst = 0;

        // Chuỗi thử: 1010
        din = 1; #10;
        din = 0; #10;
        din = 1; #10;
        din = 0; #10;  // tại đây dout phải = 1

        // Thử thêm vài chuỗi khác
        din = 1; #10;
        din = 1; #10;
        din = 0; #10;
        din = 1; #10;

        #20 $finish;
    end
endmodule
