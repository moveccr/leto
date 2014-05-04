<?php
//
// Leto060 genops.php
// Copyright (C) 2014 isaki@NetBSD.org 
//
// instructions.txt, ops.txt からディスパッチャ部分を生成
//
// 第1引数に instructions.txt、第2引数に ops.txt を指定する。
// 出力はカレントディレクトリの ops.cpp および ops.h とかの予定。
//

	init_eainfo();

	if ($argc < 2) {
		print "Usage: {$argv[0]} <instructions.txt> <ops.txt>\n";
		exit(1);
	}

	// instructions.txt を処理
	$fp = fopen($argv[1], "r");
	if ($fp === false) {
		print "fopen argv[1] failed: {$argv[1]}\n";
		exit(1);
	}
	$table = array();
	// 呼び出し先でグローバル変数 $table に直接代入して帰ってくる
	parse_instructions($fp);
	fclose($fp);

	// ops.txt を処理
	$fp = fopen($argv[2], "r");
	if ($fp === false) {
		print "fopen argv[2] failed: {$argv[2]}\n";
		exit(1);
	}
	$ops = parse_ops($fp);
	fclose($fp);

	// ジャンプテーブルを出力
	output_jumptable();

	exit(0);
?>
<?php
// instructions.txt をパースしてディスパッチャ情報(配列)を返す。
// 返す配列は以下を 0x0000 から 0xffff まで並べたもの。
//	[0x0000] => array(
//		"bits" => "0000000000000000",
//		"mpu"  => "123456",
//		"addr" => ...
//	)
function parse_instructions($fp)
{
	global $table;

	$ops060 = array();

	while (($line = fgets($fp))) {
		$line = trim(preg_replace("/;.*/", "", $line));
		if ($line == "") {
			continue;
		}

		// 1行はこんな感じ
		// 00000000ssmmmrrr|012346|D M+-WXZ  |ori_S_EA	|ORI.bwl #<data>,<ea>
		// 列は '|'(パイプ) で区切られている。
		// $bits … 1ワード目のビットパターン
		// $mpu  … MPU ごとのサポート有無
		// $addr … 対応しているアドレッシングモード
		// $name … ニーモニック(識別子用)
		// $display_name … ニーモニック(表示用)
		// アドレッシングモードの凡例については元ページ参照

		list ($bits, $mpu, $addr, $name, $display_name) =
			preg_split("/\|/", $line, -1, PREG_SPLIT_NO_EMPTY);

		// 必ず "0"/"1" に展開する文字はここで "." に変換しておく
		$bits = preg_replace("/(ddd|nnn|xxx|yyy)/", "...", $bits);

		$op = array(
			"bits" => $bits,
			"mpu" => $mpu,
			"addr" => $addr,
			"name" => trim($name),
			"display_name" => $display_name,
		);

		// 060 命令のみ対象にする
		if (preg_match("/6/", $mpu)) {
			check_duplicate("060", $op);
			$ops060[] = $op;
		}
	}

	// デバッグ表示
	if (0) {
		printf("read %d ops.\n", count($ops060));
		foreach ($ops060 as $op) {
			print "{$op["bits"]} : {$op["name"]}\n";
		}
		exit(1);
	}

	// 記載順に展開 (後に現れたほうで上書きしていく)
	foreach ($ops060 as $op) {
		$original_op = $op;		// デバッグ表示用に展開元を保存しとく
		$depth = 0;
		expand($op);
	}
	ksort($table);
}

// 重複チェック
// $op のビットパターン定義が既存のいずれとも衝突していないことを確認する。
// 重複していればエラーを表示して終了する。
// ビットパターンはアドレッシングモードまで含めて一致すれば重複とする。
// $dupchk[$id] に $key => display_name のハッシュをセットしていく。
function check_duplicate($id, $op)
{
	global $dupchk;

	$key = "{$op["bits"]}:{$op["addr"]}";
	if (isset($dupchk[$id][$key])) {
		print "{$op["bits"]}({$op["display_name"]}): duplicate with "
			. "{$dupchk[$id][$key]}\n";
		exit(1);
	}
	$dupchk[$id][$key] = $op["display_name"];
}

// $op["bits"] のうち可変ビットを再帰的に展開。
// 展開しきったら $table[] に出力。
function expand($op)
{
	global $table;

	// 長い文字列を先に一致させること

	// まずはサイズ関連。sss は1ワード目には出現しない。
	// "s"     "SS"
	// ----    -----
	// %0 W    %00 B
	// %1 L    %01 W
	//         %10 L
	if (strstr($op["bits"], "s") !== false) {
		expand_s($op);
		return;
	}
	if (strstr($op["bits"], "SS") !== false) {
		expand_SS($op);
		return;
	}

	// アドレッシングモード (MOVEのデスティネーション部分)
	if (strpos($op["bits"], "RRRMMM") !== false) {
		$op["addr2"] = $op["addr"];
		expand_addr($op, "XX");
		return;
	}

	// アドレッシングモード (通常)
	if (strpos($op["bits"], "mmmrrr") !== false) {
		expand_addr($op, "EA");
		return;
	}

	// Bcc の cccc は %0000,%0001 を含まない
	if (strpos($op["bits"], "cccc") !== false) {
		expand_cccc($op);
		return;
	}

	// "." は無条件で "0"/"1" に置換できるビット
	if (strpos($op["bits"], ".") !== false) {
		expand_bit($op, "\\.", array("0", "1"));
		return;
	}

	// 全部展開しきったはず
	$ch = count_chars($op["bits"], 0);
	if ($ch[0x30] + $ch[0x31] != 16) {
		print "\$op(\"{$op["bits"]}\", {$op["name"]}) has variable bits\n";
		exit(1);
	}

	$table[bindec($op["bits"])] = $op;
}

// $op["bits"] のうち $mask で示される変数ビットを $list に展開して
// expand() を再帰的に呼び出す。
function expand_bit($op, $mask, $list)
{
	$op2 = $op;
	foreach ($list as $dstbit) {
		$op2["bits"] = preg_replace("/{$mask}/", $dstbit, $op["bits"], 1);
		expand($op2);
	}
}

// $op["bits"] のうち "s" をサイズに置換して expand() を再帰実行する。
// その際 $op["size"] に "w", "l" を代入する。
function expand_s($op)
{
	$sizeinfo = array(
		array("pattern" => "0", "size" => "w"),
		array("pattern" => "1", "size" => "l"),
	);
	foreach ($sizeinfo as $s) {
		$op2 = $op;
		$op2["bits"] = preg_replace("/s/", $s["pattern"], $op2["bits"], 1);
		$op2["name"] = preg_replace("/S/", $s["size"], $op2["name"]);
		$op2["size"] = $s["size"];
		expand($op2);
	}
}

// $op["bits"] のうち "SS" をサイズに置換して expand() を再帰実行する。
// その際 $op["size"] に "b", "w", "l" を代入する。
function expand_SS($op)
{
	$sizeinfo = array(
		array("pattern" => "00", "size" => "b"),
		array("pattern" => "01", "size" => "w"),
		array("pattern" => "10", "size" => "l"),
	);
	foreach ($sizeinfo as $s) {
		$op2 = $op;
		$op2["bits"] = preg_replace("/SS/", $s["pattern"], $op2["bits"], 1);
		$op2["name"] = preg_replace("/S/", $s["size"], $op2["name"]);
		$op2["size"] = $s["size"];
		expand($op2);
	}
}

// expand_addr() で使用する $eainfo を作成する。
function init_eainfo()
{
	global $eainfo;

	//  +-------- アドレッシングモード記号
	//  |  +----- "reg"(レジスタ) / "mem"(メモリ)
	//  |  |   +- mmm
	//  |  |   |  rrr のカンマ区切りリスト
	$eatext = <<<__EOM__
		D reg 000 000,001,010,011,100,101,110,111
		A reg 001 000,001,010,011,100,101,110,111
		M mem 010 000,001,010,011,100,101,110,111
		+ mem 011 000,001,010,011,100,101,110,111
		- mem 100 000,001,010,011,100,101,110,111
		W mem 101 000,001,010,011,100,101,110,111
		X mem 110 000,001,010,011,100,101,110,111
		Z mem 111 000,001
		P mem 111 010,011
		I mem 111 100,101
__EOM__;
	// このテキストを分解して $eainfo を用意する
	foreach (preg_split("/\n/", $eatext) as $line) {
		list ($addr, $mode, $mmm, $rrr) = preg_split("/\s+/", $line, 4,
			PREG_SPLIT_NO_EMPTY);
		$rrr = preg_split("/,/", $rrr);
		$eainfo[] = array(
			"addr" => $addr,
			"mode" => $mode,
			"mmm" => $mmm,
			"rrr" => $rrr,
		);
	}
}

// $op["bits"] のうち "mmmrrr" をアドレッシングモードに置換して expand() を
// 再帰実行する。
// その際 $op["EA"] に "reg", "mem" を代入し、
// $op["addr"] を当該アドレッシングモードだけに限定する。
// 引数 $dst は "EA"(下位6bit) か "XX"(MOVE命令用の反転したやつ)。
//
// 通常のアドレッシングモードの他に、MOVE 命令のソース部のために、
// 似て非なるものがもう1セットある。以下のように適宜読み替えること。
//
//	通常		MOVEのDST		内容
//	-----------+---------------+-------
//	"EA"		"XX"			引数 $dst
//	$op["EA"]	$op["XX"]		mode の代入先
//	$op["addr"]	$op["addr2"]	アドレッシングモード
//	mmmrrr		RRRMMM			$op["bits"]のうち置換するパターン
//
function expand_addr($op, $dst)
{
	global $eainfo;

	if ($dst == "EA") {
		$m_mask = "mmm";
		$r_mask = "rrr";
		$addr = "addr";
	} else {
		$m_mask = "MMM";
		$r_mask = "RRR";
		$addr = "addr2";
	}

	foreach ($eainfo as $e) {
		// この op にアドレッシングモード $e["addr"] があれば
		if (strpos($op[$addr], $e["addr"]) !== false) {
			$op2 = $op;

			// "addr"("addr2") は自分自身のみに限定
			$op2[$addr] = $e["addr"];
			
			// "EA"("XX") を "reg"/"mem" に変換
			$op2["name"] = preg_replace("/{$dst}/", $e["mode"], $op2["name"]);

			// mmm(MMM) を置換
			$op2["bits"] = preg_replace("/{$m_mask}/", $e["mmm"], $op2["bits"]);
			$op2[$dst] = $e["mode"];

			// rrr(RRR) を展開
			expand_bit($op2, $r_mask, $e["rrr"]);
		}
	}
}

// $op["bits"] のうち "cccc" を Bcc の条件コードに展開する。
// Bcc には %0000, %0001 の2つは存在せず、ここは別の命令 BRA/BSR に
// 割り当てられているため。
function expand_cccc($op)
{
	$cccc = array(
		              "0010","0011","0100","0101","0110","0111",
		"1000","1001","1010","1011","1100","1101","1110","1111",
	);

	foreach ($cccc as $c) {
		$op2 = $op;
		$op2["bits"] = preg_replace("/cccc/", $c, $op2["bits"]);
		expand($op2);
	}
}

//
// ops.txt の処理
//
function parse_ops($fp)
{
	// $src = array(
	//	["ori_ccr"] => array("text" => text),
	//	["ori_sr"]  => array("text" => text),
	//  :
	// )

	$ops = array();
	$opname = "";
	$text = "";
	for ($line = 1; ($buf = fgets($fp)) !== false; $line++) {
		if ($opname == "") {
			if (trim($buf) == "") {
				continue;
			}
			if (preg_match("/^BEGIN\s+(\S+)/", $buf, $m)) {
				$opname = $m[1];
				$text = "";
			} else {
				print "syntax error in {$line}\n";
				exit(1);
			}
		} else {
			if (preg_match("/^END/", $buf)) {
				$ops[$opname] = array(
					"text" => $text,
				);
				$text = "";
				$opname = "";
			} else {
				$text .= $buf;
			}
		}
	}
	if ($opname != "") {
		print "syntax error: no END statement at file end\n";
		exit(1);
	}

	return $ops;
}

//
// ジャンプテーブルを出力
//
function output_jumptable()
{
	global $table;
	global $ops;

	$out = "";

	$out .= "OpFunc_t OpTable[65536] = {\n";
	for ($i = 0; $i < 65536; $i++) {
		if ($i % 4 == 0) $out .= " ";
		if (isset($table[$i])) {
			$funcname = "Op_{$table[$i]["name"]},";
		} else {
			$funcname = "Op_invalid,";
		}
		$out .= sprintf(" %-16s", $funcname);

		if ($i % 4 == 3) {
			$i0 = $i - 3;
			$out .= sprintf(" // \$%04x\n", $i0);
		}
	}
	$out .= "};\n";

	print $out;
}

?>
