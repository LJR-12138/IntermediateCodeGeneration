package com.compiler.backend.parser;

import com.compiler.backend.model.ReductionRule;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Collectors;

public class ReductionParser {

    private static final Pattern RULE_PATTERN =
            Pattern.compile("^(\\d+)\\.\\s+#(\\d+)\\s+(\\S+)\\s+->\\s+(.*)$");

    public List<ReductionRule> parse(String filePath) throws IOException {
        List<ReductionRule> rules = new ArrayList<>();

        try (BufferedReader reader = new BufferedReader(new FileReader(filePath))) {
            String line;
            while ((line = reader.readLine()) != null) {
                line = line.trim();
                if (line.isEmpty()) {
                    continue;
                }
                Matcher m = RULE_PATTERN.matcher(line);
                if (m.matches()) {
                    ReductionRule rule = new ReductionRule();
                    rule.setSequence(Integer.parseInt(m.group(1)));
                    rule.setId(Integer.parseInt(m.group(2)));
                    rule.setLhs(m.group(3));
                    rule.setRhs(parseRhs(m.group(4)));
                    rules.add(rule);
                }
            }
        }
        return rules;
    }

    private List<String> parseRhs(String rhs) {
        if (rhs.trim().isEmpty()) {
            return new ArrayList<>();
        }
        return Arrays.stream(rhs.trim().split("\\s+"))
                .filter(s -> !s.isEmpty())
                .collect(Collectors.toList());
    }
}
