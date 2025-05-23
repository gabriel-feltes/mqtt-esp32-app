import { createClient } from "@supabase/supabase-js";
import 'dotenv/config';

const supabaseUrl = process.env.SUPABASE_URL;
const supabaseKey = process.env.SUPABASE_KEY;

if (!supabaseUrl || !supabaseKey) {
  throw new Error("❌ SUPABASE_URL ou SUPABASE_KEY não estão definidas.");
}

export const supabase = createClient(supabaseUrl, supabaseKey);